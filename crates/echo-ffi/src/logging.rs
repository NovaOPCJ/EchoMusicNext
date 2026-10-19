//! Rust log queue -> Qt logging. Drain callbacks are synchronous and borrowed;
//! no native producer calls into Qt or retains a host context.
use std::{
    ffi::c_void,
    panic::{AssertUnwindSafe, catch_unwind},
};
#[repr(C)]
pub struct EchoLogEntry {
    pub timestamp_ms: u64,
    pub level: u32,
    pub target: *const u8,
    pub target_len: usize,
    pub message: *const u8,
    pub message_len: usize,
}
/// Process-wide Qt host chooses level: error=0/warn=1/info=2/debug=3/verbose=4.
#[unsafe(no_mangle)]
pub extern "C" fn echo_log_start(level: u32) -> bool {
    catch_unwind(|| echo_log::start(level)).unwrap_or(false)
}
/// Change the process logging threshold without discarding accepted records.
#[unsafe(no_mangle)]
pub extern "C" fn echo_log_set_level(level: u32) -> bool {
    catch_unwind(|| echo_log::set_level(level)).unwrap_or(false)
}
/// Opt-in, bounded, redacted API body diagnostics. Headers, request credentials,
/// signed URLs and raw non-JSON payloads are never written to the log queue.
#[unsafe(no_mangle)]
pub extern "C" fn echo_log_api_responses(enabled: bool) {
    echo_api::util::response_logging::set_enabled(enabled);
}
/// Drain up to 64 entries. All buffers and entry pointers are borrowed until the
/// callback returns. Host dispatches to its Qt logger on this calling thread.
/// # Safety
/// callback/context must remain valid for this call. Never recurse into drain.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn echo_log_drain(
    callback: Option<unsafe extern "C" fn(*mut c_void, *const EchoLogEntry)>,
    context: *mut c_void,
) -> usize {
    catch_unwind(AssertUnwindSafe(|| {
        let Some(callback) = callback else { return 0 };
        let entries = echo_log::drain();
        for entry in &entries {
            let record = EchoLogEntry {
                timestamp_ms: entry.timestamp_ms,
                level: entry.level,
                target: entry.target.as_ptr(),
                target_len: entry.target.len(),
                message: entry.message.as_ptr(),
                message_len: entry.message.len(),
            };
            // SAFETY: caller guarantees a synchronous callback/context; record
            // and strings remain alive throughout the callback invocation.
            unsafe { callback(context, &record) };
        }
        entries.len()
    }))
    .unwrap_or(0)
}
/// Stop only after native workers stop and Qt drains all remaining records.
#[unsafe(no_mangle)]
pub extern "C" fn echo_log_stop() {
    let _ = catch_unwind(echo_log::stop);
}
