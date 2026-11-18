//! Prints how long typical and hostile formulas take: cargo run --release --example measure

use peerchalk_math::layout_guarded;
use std::time::Instant;

const BLACK: u32 = 0x0000_00ff;
const TOLERANCE: f32 = 0.002;

fn report(name: &str, tex: &str) {
    let start = Instant::now();
    let result = layout_guarded(tex.as_bytes(), true, BLACK, TOLERANCE);
    let ms = start.elapsed().as_secs_f64() * 1000.0;
    match result {
        Ok(f) => println!("{name:<14} {ms:>9.2} ms  {:>7} triangles", f.mesh.indices.len() / 3),
        Err(error) => {
            let short: String = error.chars().take(50).collect();
            println!("{name:<14} {ms:>9.2} ms  rejected: {short}");
        }
    }
}

fn main() {
    report("fraction", r"\frac{a+b}{c}");
    report("quadratic", r"x = \frac{-b \pm \sqrt{b^2-4ac}}{2a}");
    report("integral", r"\int_{-\infty}^{\infty} e^{-x^2}\,dx = \sqrt{\pi}");
    report("sum", r"\sum_{n=1}^{\infty}\frac{1}{n^2}=\frac{\pi^2}{6}");
    report("matrix", r"\begin{bmatrix}a&b&c\\d&e&f\\g&h&i\end{bmatrix}^{-1}");
    report("aligned", r"\begin{aligned}\nabla\cdot E&=\frac{\rho}{\varepsilon_0}\\\nabla\times B&=\mu_0 J+\mu_0\varepsilon_0\frac{\partial E}{\partial t}\end{aligned}");
    report("long 16 KiB", &"x+".repeat(8_000));
    report("wide 10", &(String::from(r"\begin{matrix}") + &"x&".repeat(10) + r"x\end{matrix}"));
    report("wide 500", &(String::from(r"\begin{matrix}") + &"x&".repeat(500) + r"x\end{matrix}"));
    report("wide matrix", &(String::from(r"\begin{matrix}") + &"x&".repeat(5_000) + r"x\end{matrix}"));
    report("big matrix", &(String::from(r"\begin{matrix}") + &"x&x&x&x\\\\".repeat(1_000) + r"\end{matrix}"));
    report("many roots", &r"\sqrt{x}".repeat(2_000));
    report("many big ops", &r"\sum\int".repeat(1_500));
    report("deep braces", &"{".repeat(8_000));
    report("deep fractions", &"\\frac{".repeat(2_000));
    report("macro loop", r"\def\a{\a\a}\a");
    report("macro bomb", r"\def\a{xxxxxxxx}\def\b{\a\a\a\a\a\a\a\a}\def\c{\b\b\b\b\b\b\b\b}\def\d{\c\c\c\c\c\c\c\c}\d\d\d\d\d\d\d\d");
}
