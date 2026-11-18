use super::*;

const BLACK: u32 = 0x0000_00ff;
const TOLERANCE: f32 = 0.002;

fn formula(tex: &str) -> Formula {
    layout_formula(tex, true, BLACK, TOLERANCE).unwrap_or_else(|error| panic!("{tex}: {error}"))
}

#[test]
fn draws_common_formulas_inside_their_box() {
    for tex in [r"\frac{a}{b}", r"\sqrt{x^2+1}", r"\sum_{i=0}^n i", r"\int_0^1 f(x)\,dx",
                r"\begin{pmatrix}1&2\\3&4\end{pmatrix}", r"\left(\frac{1}{2}\right)", r"\hat{x}"] {
        let result = formula(tex);
        assert!(result.width > 0.0, "{tex}");
        assert!(!result.mesh.indices.is_empty(), "{tex}");
        assert_eq!(result.mesh.indices.len() % 3, 0, "{tex}");
        let vertex_count = (result.mesh.vertices.len() / 2) as u32;
        assert!(result.mesh.indices.iter().all(|index| *index < vertex_count), "{tex}");
        for point in result.mesh.vertices.chunks_exact(2) {
            assert!(point[0] >= -0.5 && f64::from(point[0]) <= result.width + 0.5, "{tex}");
            let bottom = result.height + result.depth + 0.5;
            assert!(point[1] >= -0.5 && f64::from(point[1]) <= bottom, "{tex}");
        }
    }
}

#[test]
fn identical_input_gives_identical_triangles() {
    let (first, second) = (formula(r"\frac{x}{y}"), formula(r"\frac{x}{y}"));
    assert_eq!(first.mesh.vertices, second.mesh.vertices);
    assert_eq!(first.mesh.indices, second.mesh.indices);
}

#[test]
fn colours_become_separate_runs() {
    let result = formula(r"a\textcolor{red}{b}c");
    assert_eq!(result.mesh.runs.len(), 3);
    assert_eq!(result.mesh.runs[0].rgba, BLACK);
    assert_eq!(result.mesh.runs[1].rgba, 0xff00_00ff);
    let total: u32 = result.mesh.runs.iter().map(|run| run.index_count).sum();
    assert_eq!(total as usize, result.mesh.indices.len());
}

#[test]
fn bad_input_is_an_error() {
    assert!(layout_formula(r"\frac{", true, BLACK, TOLERANCE).is_err());
    assert!(layout_formula(r"\nosuchcommand", true, BLACK, TOLERANCE).is_err());
    assert!(layout_formula("x", true, BLACK, f32::NAN).is_err());
    assert!(layout_formula(&"x".repeat(MAX_TEX_BYTES + 1), true, BLACK, TOLERANCE).is_err());
    assert!(layout_guarded(&[0xff, 0xfe], true, BLACK, TOLERANCE).is_err());
}

#[test]
fn hostile_formulas_end_without_crashing() {
    let inputs = [
        "{".repeat(10_000),
        "\\frac{".repeat(2_000),
        "\\sqrt{".repeat(2_000),
        "x^".repeat(5_000),
        r"\def\a{\a\a}\a".to_owned(),
        r"\def\a{xxxxxxxxxx}\def\b{\a\a\a\a\a\a\a\a}\def\c{\b\b\b\b\b\b\b\b}\c\c\c\c\c\c\c\c".to_owned(),
        r"\begin{matrix}".to_owned() + &"x&".repeat(5_000),
        "\u{0}\u{202e}\u{1F600}".to_owned(),
    ];
    for tex in &inputs {
        let _ = layout_guarded(tex.as_bytes(), true, BLACK, TOLERANCE);
    }
}

#[test]
fn panics_become_errors() {
    let result = guarded(|| panic!("boom"));
    let error = result.expect_err("the panic must be reported");
    assert!(error.contains("boom"), "{error}");
}

#[test]
fn c_interface_reports_results_and_errors() {
    // SAFETY: both pointers are valid for the given lengths and each mesh is freed once.
    unsafe {
        let tex = r"\frac12";
        let mesh = peerchalk_math_layout(tex.as_ptr().cast(), tex.len(), 1, BLACK, TOLERANCE);
        assert!((*mesh).error.is_null());
        assert!((*mesh).index_count > 0 && (*mesh).run_count == 1);
        peerchalk_math_free(mesh);
        let bad = r"\frac{";
        let mesh = peerchalk_math_layout(bad.as_ptr().cast(), bad.len(), 0, BLACK, TOLERANCE);
        assert!(!(*mesh).error.is_null());
        assert!((*mesh).vertices.is_null());
        peerchalk_math_free(mesh);
        peerchalk_math_free(ptr::null_mut());
    }
}

#[test]
fn wire_format_lays_out_counts_then_data() {
    let result = formula("x");
    let bytes = wire::encode(&Ok(result));
    let word = |at: usize| u32::from_le_bytes(bytes[at..at + 4].try_into().unwrap()) as usize;
    let (vertices, indices, runs) = (word(0), word(4), word(8));
    assert_eq!(word(12), 0);
    assert_eq!(bytes.len(), wire::HEADER_BYTES + 8 * vertices + 4 * indices + 12 * runs);
}

#[test]
fn wire_format_carries_errors() {
    let bytes = wire::encode(&Err("bad".to_owned()));
    assert_eq!(&bytes[12..16], &3u32.to_le_bytes());
    assert_eq!(&bytes[wire::HEADER_BYTES..], b"bad");
}
