use std::ffi::{CStr, CString};
use std::io::Write;
use std::sync::atomic::{AtomicI32, Ordering};
use std::sync::OnceLock;

use nix::unistd::{Uid, User};

pub use libc::{LOG_DEBUG, LOG_ERR, LOG_INFO};

const FMT_STR: &[u8] = b"%s\0";

static LOG_LEVEL: AtomicI32 = AtomicI32::new(LOG_ERR);
static PROGNAME: OnceLock<&'static CStr> = OnceLock::new();

/// The ident most recently handed to `openlog()`, so a test can check that
/// syslog always holds the name `PROGNAME` keeps alive.
#[cfg(test)]
static OPENLOG_IDENT: std::sync::atomic::AtomicPtr<libc::c_char> =
    std::sync::atomic::AtomicPtr::new(std::ptr::null_mut());

/// Open syslog under `progname`.
///
/// `progname` is `'static` because `openlog()` may keep the pointer rather
/// than copy the string (glibc does) and read it on every later `syslog()`,
/// so it must never be freed. A repeat call keeps the first name, so syslog
/// and stderr always agree.
#[allow(unsafe_code)] // libc::openlog FFI — ident is 'static, flags are constants
pub fn init(progname: &'static CStr) {
    let ident = *PROGNAME.get_or_init(|| progname);
    #[cfg(test)]
    OPENLOG_IDENT.store(ident.as_ptr().cast_mut(), Ordering::Relaxed);
    unsafe {
        libc::openlog(
            ident.as_ptr(),
            libc::LOG_CONS | libc::LOG_PID,
            libc::LOG_AUTHPRIV,
        );
    }
}

pub fn set_level(level: i32) {
    LOG_LEVEL.store(level, Ordering::Relaxed);
}

pub fn level() -> i32 {
    LOG_LEVEL.load(Ordering::Relaxed)
}

fn username() -> String {
    match User::from_uid(Uid::current()) {
        Ok(Some(u)) => u.name,
        _ => "Unknown user".to_string(),
    }
}

#[allow(unsafe_code)] // libc::syslog FFI — format is hardcoded "%s\0", arg is CString
fn write_syslog(priority: i32, message: &str) {
    // The message is passed as an argument to the constant "%s" format
    // string, so user-controlled content (usernames, command names) is
    // never interpreted as a format string and needs no escaping.
    let full = format!("{}: {}", username(), message);
    let Ok(c_msg) = CString::new(full) else {
        return;
    };
    unsafe {
        libc::syslog(priority, FMT_STR.as_ptr() as *const _, c_msg.as_ptr());
    }
}

fn write_stderr(priority: i32, message: &str) {
    if priority > level() {
        return;
    }
    let prog = PROGNAME
        .get()
        .and_then(|c| c.to_str().ok())
        .unwrap_or("root");
    let _ = writeln!(std::io::stderr(), "{prog}: {message}");
}

pub fn log(priority: i32, message: &str) {
    write_syslog(priority, message);
    write_stderr(priority, message);
}

#[macro_export]
macro_rules! debug {
    ($($arg:tt)*) => {
        $crate::logging::log($crate::logging::LOG_DEBUG, &format!($($arg)*))
    };
}

#[macro_export]
macro_rules! info {
    ($($arg:tt)*) => {
        $crate::logging::log($crate::logging::LOG_INFO, &format!($($arg)*))
    };
}

#[macro_export]
macro_rules! error {
    ($($arg:tt)*) => {
        $crate::logging::log($crate::logging::LOG_ERR, &format!($($arg)*))
    };
}

/// Print to stderr without going through syslog and without adding a newline.
#[macro_export]
macro_rules! print_stderr {
    ($($arg:tt)*) => {{
        use std::io::Write;
        let _ = write!(std::io::stderr(), $($arg)*);
    }};
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn level_default_and_set() {
        let original = level();
        set_level(LOG_ERR);
        assert_eq!(level(), LOG_ERR);
        set_level(LOG_DEBUG);
        assert_eq!(level(), LOG_DEBUG);
        set_level(original);
    }

    #[test]
    fn repeat_init_keeps_syslog_on_the_stored_name() {
        // openlog() may keep the ident pointer, so after any number of init
        // calls it must hold the name PROGNAME keeps alive, never one a
        // later call passed in and the caller may free.
        init(c"roottest");
        init(c"roottest-again");
        let stored = *PROGNAME.get().unwrap();
        assert_eq!(stored, c"roottest");
        assert_eq!(
            OPENLOG_IDENT.load(Ordering::Relaxed).cast_const(),
            stored.as_ptr()
        );
        log(LOG_DEBUG, "logging test after repeated init");
    }
}
