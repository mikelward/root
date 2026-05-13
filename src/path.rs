use std::ffi::OsStr;
use std::os::unix::ffi::OsStrExt;
use std::path::{Path, PathBuf};

use nix::unistd::{access, AccessFlags};

pub const DIRSEP: u8 = b'/';
pub const PATHENVSEP: u8 = b':';

pub fn is_absolute_path(path: &OsStr) -> bool {
    path.as_bytes().first() == Some(&DIRSEP)
}

pub fn is_qualified_path(path: &OsStr) -> bool {
    path.as_bytes().contains(&DIRSEP)
}

#[allow(dead_code)]
pub fn is_unqualified_path(path: &OsStr) -> bool {
    !path.as_bytes().contains(&DIRSEP)
}

/// Iterate over each entry of a PATH-style environment value.
///
/// Empty entries are reported as `.` (POSIX semantics).
pub fn pathenv_each<F: FnMut(&OsStr)>(pathenv: &OsStr, mut func: F) {
    for entry in pathenv.as_bytes().split(|b| *b == PATHENVSEP) {
        let dir: &[u8] = if entry.is_empty() { b"." } else { entry };
        func(OsStr::from_bytes(dir));
    }
}

/// Search `pathenv` for `command`. Return the first directory entry concatenated
/// with `command` that is executable (`X_OK`). Returns `None` if no match.
///
/// The returned path may be relative if the matching PATH entry was relative;
/// the caller is responsible for verifying safety via `is_absolute_path`.
pub fn get_command_path(command: &OsStr, pathenv: &OsStr) -> Option<PathBuf> {
    for entry in pathenv.as_bytes().split(|b| *b == PATHENVSEP) {
        let dir: &[u8] = if entry.is_empty() { b"." } else { entry };
        let candidate = Path::new(OsStr::from_bytes(dir)).join(command);
        if access(&candidate, AccessFlags::X_OK).is_ok() {
            return Some(candidate);
        }
    }
    None
}

#[cfg(test)]
mod tests {
    use super::*;

    fn os(s: &str) -> &OsStr {
        OsStr::new(s)
    }

    fn collect(pathenv: &str) -> Vec<String> {
        let mut out = Vec::new();
        pathenv_each(os(pathenv), |d| {
            out.push(d.to_string_lossy().into_owned());
        });
        out
    }

    #[test]
    fn pathenv_each_basic() {
        assert_eq!(collect("/usr/bin:/bin"), vec!["/usr/bin", "/bin"]);
    }

    #[test]
    fn pathenv_each_single() {
        assert_eq!(collect("/usr/bin"), vec!["/usr/bin"]);
    }

    #[test]
    fn pathenv_each_empty_start() {
        assert_eq!(collect(":/usr/bin"), vec![".", "/usr/bin"]);
    }

    #[test]
    fn pathenv_each_empty_end() {
        assert_eq!(collect("/usr/bin:"), vec!["/usr/bin", "."]);
    }

    #[test]
    fn pathenv_each_empty_middle() {
        assert_eq!(collect("/usr/bin::/bin"), vec!["/usr/bin", ".", "/bin"]);
    }

    #[test]
    fn pathenv_each_all_empty() {
        assert_eq!(collect("::"), vec![".", ".", "."]);
    }

    #[test]
    fn absolute_paths() {
        assert!(is_absolute_path(os("/bin/ls")));
        assert!(is_absolute_path(os("/")));
        assert!(!is_absolute_path(os("bin/ls")));
        assert!(!is_absolute_path(os("./ls")));
        assert!(!is_absolute_path(os("")));
    }

    #[test]
    fn qualified_paths() {
        assert!(is_qualified_path(os("/bin/ls")));
        assert!(is_qualified_path(os("./ls")));
        assert!(is_qualified_path(os("../bin/ls")));
        assert!(!is_qualified_path(os("ls")));
        assert!(!is_qualified_path(os("")));
    }

    #[test]
    fn unqualified_paths() {
        assert!(is_unqualified_path(os("ls")));
        assert!(!is_unqualified_path(os("/bin/ls")));
        assert!(!is_unqualified_path(os("./ls")));
        assert!(is_unqualified_path(os("")));
    }

    #[test]
    fn non_utf8_path_is_qualified() {
        use std::ffi::OsString;
        use std::os::unix::ffi::OsStringExt;
        // "./" + 0xFF — contains '/', so it's qualified.
        let s = OsString::from_vec(vec![b'.', b'/', 0xFF]);
        assert!(is_qualified_path(&s));
        assert!(!is_absolute_path(&s));
    }
}
