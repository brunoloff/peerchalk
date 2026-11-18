//! Entry points used by web/math-worker.js. A worker handles one formula at a time.

use std::cell::RefCell;
use std::ffi::c_int;

use crate::{layout_guarded, wire};

thread_local! {
    static INPUT: RefCell<Vec<u8>> = const { RefCell::new(Vec::new()) };
    static OUTPUT: RefCell<Vec<u8>> = const { RefCell::new(Vec::new()) };
}

/// Makes room for a formula of `size` bytes, to be written at the returned address.
#[no_mangle]
pub extern "C" fn peerchalk_worker_input(size: usize) -> *mut u8 {
    INPUT.with_borrow_mut(|input| {
        input.clear();
        input.resize(size, 0);
        input.as_mut_ptr()
    })
}

/// Lays out the formula and returns the address of the result.
#[no_mangle]
pub extern "C" fn peerchalk_worker_layout(display: c_int, rgba: u32, tolerance: f32) -> *const u8 {
    let result = INPUT.with_borrow(|tex| layout_guarded(tex, display != 0, rgba, tolerance));
    OUTPUT.with_borrow_mut(|output| {
        *output = wire::encode(&result);
        output.as_ptr()
    })
}

#[no_mangle]
pub extern "C" fn peerchalk_worker_output_size() -> usize {
    OUTPUT.with_borrow(Vec::len)
}
