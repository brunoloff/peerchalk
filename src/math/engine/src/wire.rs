//! The result format sent from the browser's math worker to the app, little-endian:
//! u32 vertex, index and run counts and error length; f64 width, height and depth; then
//! the vertices (2 × f32), indices (u32), runs (3 × u32) and the UTF-8 error.

use crate::Formula;

pub const HEADER_BYTES: usize = 40;

fn push_u32(out: &mut Vec<u8>, value: usize) {
    out.extend_from_slice(&(value as u32).to_le_bytes());
}

pub fn encode(result: &Result<Formula, String>) -> Vec<u8> {
    let (formula, error) = match result {
        Ok(formula) => (Some(formula), ""),
        Err(error) => (None, error.as_str()),
    };
    let mesh = formula.map(|formula| &formula.mesh);
    let vertices = mesh.map_or(&[][..], |mesh| &mesh.vertices);
    let indices = mesh.map_or(&[][..], |mesh| &mesh.indices);
    let runs = mesh.map_or(&[][..], |mesh| &mesh.runs);
    let size = vertices.len() * 4 + indices.len() * 4 + runs.len() * 12 + error.len();
    let mut out = Vec::with_capacity(HEADER_BYTES + size);
    push_u32(&mut out, vertices.len() / 2);
    push_u32(&mut out, indices.len());
    push_u32(&mut out, runs.len());
    push_u32(&mut out, error.len());
    for value in formula.map_or([0.0; 3], |f| [f.width, f.height, f.depth]) {
        out.extend_from_slice(&value.to_le_bytes());
    }
    vertices.iter().for_each(|value| out.extend_from_slice(&value.to_le_bytes()));
    indices.iter().for_each(|value| out.extend_from_slice(&value.to_le_bytes()));
    for run in runs {
        for value in [run.first_index, run.index_count, run.rgba] {
            out.extend_from_slice(&value.to_le_bytes());
        }
    }
    out.extend_from_slice(error.as_bytes());
    out
}
