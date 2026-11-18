//! Converts a RaTeX display list into triangles. Glyph outlines come from the KaTeX fonts
//! compiled into this library, never from the system, so every device draws the same shapes.

use std::collections::HashMap;

use lyon_tessellation::math::{point, Point};
use lyon_tessellation::path::path::Builder;
use lyon_tessellation::path::Path;
use lyon_tessellation::{
    BuffersBuilder, FillOptions, FillTessellator, FillVertex, StrokeOptions, StrokeTessellator,
    StrokeVertex, VertexBuffers,
};
use ratex_font::{katex_ttf_glyph_char, FontId};
use ratex_types::color::Color;
use ratex_types::display_item::{DisplayItem, DisplayList};
use ratex_types::path_command::PathCommand;
use ttf_parser::{Face, OutlineBuilder};

/// Refuses formulas whose mesh would be unreasonable to keep or draw.
pub const MAX_VERTICES: usize = 250_000;
const UNFILLED_PATH_WIDTH_EM: f32 = 0.0375;

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct Run {
    pub first_index: u32,
    pub index_count: u32,
    pub rgba: u32,
}

#[derive(Debug, Default)]
pub struct Mesh {
    pub vertices: Vec<f32>,
    pub indices: Vec<u32>,
    pub runs: Vec<Run>,
}

fn rgba(color: &Color) -> u32 {
    let channel = |value: f32| (value.clamp(0.0, 1.0) * 255.0).round() as u32;
    channel(color.r) << 24 | channel(color.g) << 16 | channel(color.b) << 8 | channel(color.a)
}

/// Feeds a font outline, in font units, into a path in em.
struct GlyphPath {
    builder: Builder,
    x: f32,
    y: f32,
    scale: f32,
    open: bool,
}

impl GlyphPath {
    fn at(&self, x: f32, y: f32) -> Point {
        point(self.x + x * self.scale, self.y - y * self.scale)
    }
    fn finish_contour(&mut self) {
        if self.open {
            self.builder.end(true);
            self.open = false;
        }
    }
}

impl OutlineBuilder for GlyphPath {
    fn move_to(&mut self, x: f32, y: f32) {
        self.finish_contour();
        let at = self.at(x, y);
        self.builder.begin(at);
        self.open = true;
    }
    fn line_to(&mut self, x: f32, y: f32) {
        let at = self.at(x, y);
        self.builder.line_to(at);
    }
    fn quad_to(&mut self, x1: f32, y1: f32, x: f32, y: f32) {
        let (control, to) = (self.at(x1, y1), self.at(x, y));
        self.builder.quadratic_bezier_to(control, to);
    }
    fn curve_to(&mut self, x1: f32, y1: f32, x2: f32, y2: f32, x: f32, y: f32) {
        let (first, second, to) = (self.at(x1, y1), self.at(x2, y2), self.at(x, y));
        self.builder.cubic_bezier_to(first, second, to);
    }
    fn close(&mut self) {
        self.finish_contour();
    }
}

/// The compiled-in fonts, parsed once per formula as they are needed.
#[derive(Default)]
struct Fonts {
    faces: HashMap<FontId, Option<Face<'static>>>,
}

impl Fonts {
    fn face(&mut self, id: FontId) -> Option<&Face<'static>> {
        self.faces.entry(id).or_insert_with(|| load_face(id)).as_ref()
    }
}

fn load_face(id: FontId) -> Option<Face<'static>> {
    let file = format!("KaTeX_{}.ttf", id.as_str());
    match ratex_katex_fonts::ttf_bytes(&file)? {
        std::borrow::Cow::Borrowed(bytes) => Face::parse(bytes, 0).ok(),
        std::borrow::Cow::Owned(_) => None,
    }
}

/// The outline of a glyph, or None for characters the math fonts do not have.
fn glyph_path(fonts: &mut Fonts, font: &str, code: u32, x: f32, y: f32, scale: f32) -> Option<Path> {
    let id = FontId::parse(font).unwrap_or(FontId::MainRegular);
    let face = fonts.face(id)?;
    let glyph = face.glyph_index(katex_ttf_glyph_char(id, code))?;
    let scale = scale / f32::from(face.units_per_em());
    let mut path = GlyphPath { builder: Path::builder(), x, y, scale, open: false };
    face.outline_glyph(glyph, &mut path)?;
    path.finish_contour();
    Some(path.builder.build())
}

fn rectangle(x: f32, y: f32, width: f32, height: f32) -> Path {
    let mut builder = Path::builder();
    builder.begin(point(x, y));
    builder.line_to(point(x + width, y));
    builder.line_to(point(x + width, y + height));
    builder.line_to(point(x, y + height));
    builder.end(true);
    builder.build()
}

fn command_path(x: f32, y: f32, commands: &[PathCommand]) -> Path {
    let at = |px: f64, py: f64| point(x + px as f32, y + py as f32);
    let mut builder = Path::builder();
    let mut open = false;
    for command in commands {
        match (*command, open) {
            (PathCommand::MoveTo { x, y }, _) => {
                if open {
                    builder.end(false);
                }
                builder.begin(at(x, y));
                open = true;
            }
            (PathCommand::LineTo { x, y }, true) => {
                builder.line_to(at(x, y));
            }
            (PathCommand::QuadTo { x1, y1, x, y }, true) => {
                builder.quadratic_bezier_to(at(x1, y1), at(x, y));
            }
            (PathCommand::CubicTo { x1, y1, x2, y2, x, y }, true) => {
                builder.cubic_bezier_to(at(x1, y1), at(x2, y2), at(x, y));
            }
            (PathCommand::Close, true) => {
                builder.end(true);
                open = false;
            }
            _ => {} // drawing before any MoveTo has no starting point
        }
    }
    if open {
        builder.end(false);
    }
    builder.build()
}

struct Tessellator {
    buffers: VertexBuffers<[f32; 2], u32>,
    fill: FillTessellator,
    stroke: StrokeTessellator,
    tolerance: f32,
}

impl Tessellator {
    fn fill(&mut self, path: &Path) -> Result<(), String> {
        let options = FillOptions::non_zero().with_tolerance(self.tolerance);
        let mut output = BuffersBuilder::new(&mut self.buffers, |v: FillVertex| v.position().to_array());
        self.fill
            .tessellate_path(path, &options, &mut output)
            .map(|_| ())
            .map_err(|error| format!("could not fill a shape: {error:?}"))
    }
    fn stroke(&mut self, path: &Path) -> Result<(), String> {
        let options = StrokeOptions::default()
            .with_line_width(UNFILLED_PATH_WIDTH_EM)
            .with_tolerance(self.tolerance);
        let mut output =
            BuffersBuilder::new(&mut self.buffers, |v: StrokeVertex| v.position().to_array());
        self.stroke
            .tessellate_path(path, &options, &mut output)
            .map(|_| ())
            .map_err(|error| format!("could not outline a shape: {error:?}"))
    }
}

fn add_item(out: &mut Tessellator, fonts: &mut Fonts, item: &DisplayItem) -> Result<Color, String> {
    match item {
        DisplayItem::GlyphPath { x, y, scale, font, char_code, color } => {
            let path = glyph_path(fonts, font, *char_code, *x as f32, *y as f32, *scale as f32);
            if let Some(path) = path {
                out.fill(&path)?;
            }
            Ok(*color)
        }
        DisplayItem::Line { x, y, width, thickness, color, .. } => {
            let half = *thickness as f32 / 2.0;
            let top = *y as f32 - half;
            out.fill(&rectangle(*x as f32, top, *width as f32, *thickness as f32))?;
            Ok(*color)
        }
        DisplayItem::Rect { x, y, width, height, color } => {
            out.fill(&rectangle(*x as f32, *y as f32, *width as f32, *height as f32))?;
            Ok(*color)
        }
        DisplayItem::Path { x, y, commands, fill, color } => {
            let path = command_path(*x as f32, *y as f32, commands);
            if *fill { out.fill(&path)? } else { out.stroke(&path)? }
            Ok(*color)
        }
    }
}

fn extend_runs(runs: &mut Vec<Run>, first_index: u32, end_index: u32, rgba: u32) {
    if first_index == end_index {
        return;
    }
    match runs.last_mut() {
        Some(last) if last.rgba == rgba => last.index_count += end_index - first_index,
        _ => runs.push(Run { first_index, index_count: end_index - first_index, rgba }),
    }
}

/// Builds the triangles of a display list.
pub fn build_mesh(list: &DisplayList, tolerance: f32) -> Result<Mesh, String> {
    let mut out = Tessellator {
        buffers: VertexBuffers::new(),
        fill: FillTessellator::new(),
        stroke: StrokeTessellator::new(),
        tolerance,
    };
    let mut fonts = Fonts::default();
    let mut runs = Vec::new();
    for item in &list.items {
        let first = out.buffers.indices.len() as u32;
        let color = add_item(&mut out, &mut fonts, item)?;
        if out.buffers.vertices.len() > MAX_VERTICES {
            return Err("the formula is too large to draw".to_owned());
        }
        extend_runs(&mut runs, first, out.buffers.indices.len() as u32, rgba(&color));
    }
    let vertices = out.buffers.vertices.into_iter().flatten().collect();
    Ok(Mesh { vertices, indices: out.buffers.indices, runs })
}
