use std::ffi::{CStr, CString, OsStr, OsString};
use std::fmt::Write as _;
use std::io::Write;
use std::os::unix::ffi::OsStrExt;
use std::sync::atomic::{AtomicI32, Ordering};
use std::sync::OnceLock;

use nix::unistd::Uid;

pub use libc::{LOG_DEBUG, LOG_ERR, LOG_INFO};

const FMT_STR: &[u8] = b"%s\0";

static LOG_LEVEL: AtomicI32 = AtomicI32::new(LOG_ERR);
static PROGNAME: OnceLock<&'static CStr> = OnceLock::new();
static CALLER: OnceLock<OsString> = OnceLock::new();

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
    // Capture the caller's name now, while the real uid is still theirs.
    caller();
}

pub fn set_level(level: i32) {
    LOG_LEVEL.store(level, Ordering::Relaxed);
}

pub fn level() -> i32 {
    LOG_LEVEL.load(Ordering::Relaxed)
}

/// The calling user's name, looked up once.
///
/// `init` captures it before anything else runs, so lines logged after
/// `setuid(0)` still name the caller rather than root.
fn caller() -> &'static OsStr {
    CALLER.get_or_init(|| match crate::user::user_name(Uid::current()) {
        Ok(Some(name)) => name,
        _ => OsString::from("Unknown user"),
    })
}

/// Text from outside the program, escaped for a message.
///
/// Command names, paths, PATH entries, option strings and user and group
/// names can hold anything but NUL, and a newline or terminal escape in
/// them could forge or hide lines in the audit log. So a backslash becomes
/// `\\`; newline, carriage return and tab become `\n`, `\r` and `\t`; any
/// other control character (U+0000 to U+001F, U+007F to U+009F), and any
/// byte that is not valid UTF-8, becomes `\xNN`, one per byte. All other
/// valid UTF-8 passes through, so each message stays one unambiguous line.
/// The C build's `escape_for_log()` follows the same rules.
pub fn escape(text: &OsStr) -> String {
    let mut out = String::with_capacity(text.len());
    for chunk in text.as_bytes().utf8_chunks() {
        for c in chunk.valid().chars() {
            match c {
                '\\' => out.push_str("\\\\"),
                '\n' => out.push_str("\\n"),
                '\r' => out.push_str("\\r"),
                '\t' => out.push_str("\\t"),
                c if c.is_control() => {
                    for &b in c.encode_utf8(&mut [0; 4]).as_bytes() {
                        push_hex_escape(&mut out, b);
                    }
                }
                c => out.push(c),
            }
        }
        for &b in chunk.invalid() {
            push_hex_escape(&mut out, b);
        }
    }
    out
}

fn push_hex_escape(out: &mut String, byte: u8) {
    // Writing to a String cannot fail.
    let _ = write!(out, "\\x{byte:02x}");
}

#[allow(unsafe_code)] // libc::syslog FFI — format is hardcoded "%s\0", arg is CString
fn write_syslog(priority: i32, message: &str) {
    // The message is passed as an argument to the constant "%s" format
    // string, so user-controlled content (usernames, command names) is
    // never interpreted as a format string and needs no escaping.
    let full = format!("{}: {}", escape(caller()), message);
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

    // The cases match the C build's escape tests in legacy/loggingtest.c, so
    // the two builds escape alike.
    fn esc(bytes: &[u8]) -> String {
        escape(OsStr::from_bytes(bytes))
    }

    #[test]
    fn escape_leaves_plain_text_alone() {
        assert_eq!(esc(b"/usr/bin/ls"), "/usr/bin/ls");
        assert_eq!(esc("/home/jos\u{e9}/bin".as_bytes()), "/home/jos\u{e9}/bin");
        assert_eq!(esc(b""), "");
        assert_eq!(esc(b"%sally"), "%sally");
    }

    #[test]
    fn escape_marks_backslashes_and_whitespace() {
        assert_eq!(esc(b"a\\b"), "a\\\\b");
        assert_eq!(esc(b"a\nb\rc\td"), "a\\nb\\rc\\td");
    }

    #[test]
    fn escape_hex_escapes_control_characters() {
        assert_eq!(esc(b"\x1b[2K"), "\\x1b[2K");
        assert_eq!(esc(b"\x01\x7f"), "\\x01\\x7f");
        // U+009B, the one-byte CSI some terminals honor, as UTF-8.
        assert_eq!(esc(b"\xc2\x9b"), "\\xc2\\x9b");
    }

    #[test]
    fn escape_hex_escapes_invalid_utf8() {
        assert_eq!(esc(b"\xff"), "\\xff");
        // A truncated sequence, an overlong form and a surrogate.
        assert_eq!(esc(b"\xe2\x82A"), "\\xe2\\x82A");
        assert_eq!(esc(b"\xc0\xaf"), "\\xc0\\xaf");
        assert_eq!(esc(b"\xed\xa0\x80"), "\\xed\\xa0\\x80");
        // A sequence cut short by the end of the string.
        assert_eq!(esc(b"\xf0\x9f\x98"), "\\xf0\\x9f\\x98");
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

    /// Set in the child process `caller_survives_setuid` starts.
    #[cfg(any(target_os = "linux", target_os = "freebsd"))]
    const CALLER_CHILD_ENV: &str = "ROOT_TEST_CALLER_CHILD";

    #[cfg(any(target_os = "linux", target_os = "freebsd"))]
    #[test]
    fn caller_survives_setuid() {
        // Changing uids is irreversible and needs root, so the work happens
        // in a child process running only the test below. CI reruns this
        // binary under sudo so it does not skip there.
        if !Uid::effective().is_root() {
            eprintln!("skipping: needs root to change uids in a child");
            return;
        }
        let out = std::process::Command::new(std::env::current_exe().unwrap())
            .args(["--exact", "logging::tests::caller_survives_setuid_child"])
            .args(["--test-threads=1", "--nocapture"])
            .env(CALLER_CHILD_ENV, "1")
            .output()
            .expect("failed to run the child test");
        let stdout = String::from_utf8_lossy(&out.stdout);
        assert!(
            out.status.success(),
            "child failed:\n{stdout}\n{}",
            String::from_utf8_lossy(&out.stderr)
        );
        assert!(stdout.contains("1 passed"), "child ran no test:\n{stdout}");
    }

    #[cfg(any(target_os = "linux", target_os = "freebsd"))]
    #[test]
    fn caller_survives_setuid_child() {
        use nix::unistd::{setresuid, setuid};

        if std::env::var_os(CALLER_CHILD_ENV).is_none() {
            return;
        }
        let name_of = |uid: Uid| match crate::user::user_name(uid) {
            Ok(Some(name)) => name,
            _ => OsString::from("Unknown user"),
        };
        let caller_uid = Uid::from_raw(65534);
        let expected = name_of(caller_uid);
        assert_ne!(expected, name_of(Uid::from_raw(0)));

        // Start where the installed setuid binary does: the real uid is the
        // caller's and the effective uid is root. Capturing the effective
        // uid would name root here.
        setresuid(caller_uid, Uid::from_raw(0), Uid::from_raw(0))
            .expect("setresuid failed");
        init(c"roottest");
        // Then become root the way become_root() does. Looking the name up
        // per message would name root from here on.
        setuid(Uid::from_raw(0)).expect("setuid(0) failed");
        assert_eq!(caller(), expected);
    }
}
