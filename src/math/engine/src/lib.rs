//! LaTeX to triangles for PeerChalk; include/peerchalk_math.h is the C interface.
//! Formulas arrive from other people, so errors and panics end here as error messages.

mod mesh;
pub mod wire;
#[cfg(target_os = "emscripten")]
mod worker;

use std::any::Any;
use std::ffi::{c_char, c_int, CString};
use std::panic::{catch_unwind, UnwindSafe};
use std::ptr;

use ratex_layout::{layout, to_display_list, validate_layout, LayoutOptions};
use ratex_parser::parse;
use ratex_types::color::Color;
use ratex_types::math_style::MathStyle;

pub use mesh::{Mesh, Run, MAX_VERTICES};

pub const MAX_TEX_BYTES: usize = 16 * 1024;
const MIN_TOLERANCE_EM: f32 = 0.0001;
const MAX_TOLERANCE_EM: f32 = 0.1;

#[derive(Debug)]
pub struct Formula {
    pub mesh: Mesh,
    pub width: f64,
    pub height: f64,
    pub depth: f64,
}

fn color_from(rgba: u32) -> Color {
    let channel = |shift: u32| ((rgba >> shift) & 0xff) as f32 / 255.0;
    Color { r: channel(24), g: channel(16), b: channel(8), a: channel(0) }
}

/// Lays out a formula and builds its triangles.
pub fn layout_formula(tex: &str, display: bool, rgba: u32, tolerance: f32) -> Result<Formula, String> {
    if tex.len() > MAX_TEX_BYTES {
        return Err(format!("the formula is longer than {MAX_TEX_BYTES} bytes"));
    }
    if !tolerance.is_finite() {
        return Err("the curve tolerance must be finite".to_owned());
    }
    let nodes = parse(tex).map_err(|error| error.to_string())?;
    let style = if display { MathStyle::Display } else { MathStyle::Text };
    let options = LayoutOptions::default().with_style(style).with_color(color_from(rgba));
    let root = layout(&nodes, &options);
    validate_layout(&root)?;
    let list = to_display_list(&root);
    let tolerance = tolerance.clamp(MIN_TOLERANCE_EM, MAX_TOLERANCE_EM);
    let mesh = mesh::build_mesh(&list, tolerance)?;
    Ok(Formula { mesh, width: list.width, height: list.height, depth: list.depth })
}

fn panic_message(payload: &(dyn Any + Send)) -> String {
    let detail = payload
        .downcast_ref::<&str>()
        .map(|text| (*text).to_owned())
        .or_else(|| payload.downcast_ref::<String>().cloned())
        .unwrap_or_else(|| "unknown cause".to_owned());
    format!("internal error in the math engine: {detail}")
}

/// Like `layout_formula`, but also turns panics into errors.
pub fn layout_guarded(tex: &[u8], display: bool, rgba: u32, tolerance: f32) -> Result<Formula, String> {
    let tex = std::str::from_utf8(tex).map_err(|_| "the formula is not valid UTF-8".to_owned())?;
    guarded(|| layout_formula(tex, display, rgba, tolerance))
}

fn guarded(work: impl FnOnce() -> Result<Formula, String> + UnwindSafe) -> Result<Formula, String> {
    catch_unwind(work).unwrap_or_else(|payload| Err(panic_message(payload.as_ref())))
}

/// C view of a mesh; `Owned` keeps what its pointers refer to alive.
#[repr(C)]
pub struct PeerChalkMathMesh {
    vertices: *const f32,
    vertex_count: usize,
    indices: *const u32,
    index_count: usize,
    runs: *const CRun,
    run_count: usize,
    width: f64,
    height: f64,
    depth: f64,
    error: *const c_char,
}

#[repr(C)]
pub struct CRun {
    first_index: u32,
    index_count: u32,
    rgba: u32,
}

#[repr(C)]
struct Owned {
    view: PeerChalkMathMesh, // first, so a pointer to it is a pointer to Owned
    vertices: Vec<f32>,
    indices: Vec<u32>,
    runs: Vec<CRun>,
    error: Option<CString>,
}

fn empty_view() -> PeerChalkMathMesh {
    PeerChalkMathMesh {
        vertices: ptr::null(),
        vertex_count: 0,
        indices: ptr::null(),
        index_count: 0,
        runs: ptr::null(),
        run_count: 0,
        width: 0.0,
        height: 0.0,
        depth: 0.0,
        error: ptr::null(),
    }
}

fn owned_from(result: Result<Formula, String>) -> Box<Owned> {
    let mut owned = Box::new(Owned {
        view: empty_view(),
        vertices: Vec::new(),
        indices: Vec::new(),
        runs: Vec::new(),
        error: None,
    });
    match result {
        Ok(formula) => {
            owned.vertices = formula.mesh.vertices;
            owned.indices = formula.mesh.indices;
            owned.runs = formula.mesh.runs.iter().map(|run| CRun {
                first_index: run.first_index,
                index_count: run.index_count,
                rgba: run.rgba,
            }).collect();
            owned.view.vertices = owned.vertices.as_ptr();
            owned.view.vertex_count = owned.vertices.len() / 2;
            owned.view.indices = owned.indices.as_ptr();
            owned.view.index_count = owned.indices.len();
            owned.view.runs = owned.runs.as_ptr();
            owned.view.run_count = owned.runs.len();
            owned.view.width = formula.width;
            owned.view.height = formula.height;
            owned.view.depth = formula.depth;
        }
        Err(message) => {
            let text = CString::new(message.replace('\0', " ")).unwrap_or_default();
            owned.view.error = text.as_ptr();
            owned.error = Some(text);
        }
    }
    owned
}

/// # Safety
/// `tex` must point to `tex_length` readable bytes, or be null with a length of zero.
#[no_mangle]
pub unsafe extern "C" fn peerchalk_math_layout(
    tex: *const c_char,
    tex_length: usize,
    display: c_int,
    rgba: u32,
    tolerance: f32,
) -> *mut PeerChalkMathMesh {
    let bytes: &[u8] = if tex.is_null() || tex_length == 0 {
        &[]
    } else {
        // SAFETY: the caller promises tex_length readable bytes at tex.
        unsafe { std::slice::from_raw_parts(tex.cast::<u8>(), tex_length) }
    };
    let owned = owned_from(layout_guarded(bytes, display != 0, rgba, tolerance));
    Box::into_raw(owned).cast::<PeerChalkMathMesh>()
}

/// # Safety
/// `mesh` must come from `peerchalk_math_layout` and not be freed twice; null is ignored.
#[no_mangle]
pub unsafe extern "C" fn peerchalk_math_free(mesh: *mut PeerChalkMathMesh) {
    if !mesh.is_null() {
        // SAFETY: mesh is the Box<Owned> released by peerchalk_math_layout.
        drop(unsafe { Box::from_raw(mesh.cast::<Owned>()) });
    }
}

#[cfg(test)]
mod tests;
