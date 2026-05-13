use std::ffi::CString;
use std::io::Write;
use std::sync::atomic::{AtomicI32, Ordering};
use std::sync::OnceLock;

use nix::unistd::{Uid, User};

pub use libc::{LOG_DEBUG, LOG_ERR, LOG_INFO};

const FMT_STR: &[u8] = b"%s\0";

static LOG_LEVEL: AtomicI32 = AtomicI32::new(LOG_ERR);
static PROGNAME: OnceLock<CString> = OnceLock::new();

pub fn init(progname: &str) {
    let cprog = CString::new(progname).expect("program name must not contain NUL");
    unsafe {
        libc::openlog(
            cprog.as_ptr(),
            libc::LOG_CONS | libc::LOG_PID,
            libc::LOG_AUTHPRIV,
        );
    }
    let _ = PROGNAME.set(cprog);
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

pub fn escape_percents(input: &str) -> String {
    input.replace('%', "%%")
}

fn write_syslog(priority: i32, message: &str) {
    let prefix = escape_percents(&username());
    let escaped_msg = escape_percents(message);
    let full = format!("{prefix}: {escaped_msg}");
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
    fn escape_percents_passthrough() {
        assert_eq!(escape_percents("mikel"), "mikel");
    }

    #[test]
    fn escape_percents_doubles_percent() {
        assert_eq!(escape_percents("%sally"), "%%sally");
    }

    #[test]
    fn escape_percents_empty() {
        assert_eq!(escape_percents(""), "");
    }

    #[test]
    fn escape_percents_multiple() {
        assert_eq!(escape_percents("%a%b%"), "%%a%%b%%");
    }

    #[test]
    fn level_default_and_set() {
        let original = level();
        set_level(LOG_ERR);
        assert_eq!(level(), LOG_ERR);
        set_level(LOG_DEBUG);
        assert_eq!(level(), LOG_DEBUG);
        set_level(original);
    }
}
