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
/// with `command` that is an executable (`X_OK`) regular file. Returns `None`
/// if no match.
///
/// Directories (and other non-regular files) are skipped even if they have
/// execute permission, so a directory named like the command does not shadow
/// a real executable in a later PATH entry.
///
/// The returned path may be relative if the matching PATH entry was relative;
/// the caller is responsible for verifying safety via `is_absolute_path`.
pub fn get_command_path(command: &OsStr, pathenv: &OsStr) -> Option<PathBuf> {
    for entry in pathenv.as_bytes().split(|b| *b == PATHENVSEP) {
        let dir: &[u8] = if entry.is_empty() { b"." } else { entry };
        let candidate = Path::new(OsStr::from_bytes(dir)).join(command);
        if is_regular_file(&candidate) && access(&candidate, AccessFlags::X_OK).is_ok() {
            return Some(candidate);
        }
    }
    None
}

/// Whether `path` is a regular file (following symlinks).
fn is_regular_file(path: &Path) -> bool {
    std::fs::metadata(path).map(|m| m.is_file()).unwrap_or(false)
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

    fn make_executable(path: &Path) {
        use std::os::unix::fs::PermissionsExt;
        std::fs::write(path, "#!/bin/sh\nexit 0\n").unwrap();
        std::fs::set_permissions(path, std::fs::Permissions::from_mode(0o755)).unwrap();
    }

    fn pathenv_of(dirs: &[&Path]) -> std::ffi::OsString {
        use std::os::unix::ffi::OsStringExt;
        let mut bytes = Vec::new();
        for (i, d) in dirs.iter().enumerate() {
            if i > 0 {
                bytes.push(PATHENVSEP);
            }
            bytes.extend_from_slice(d.as_os_str().as_bytes());
        }
        std::ffi::OsString::from_vec(bytes)
    }

    #[test]
    fn get_command_path_finds_executable() {
        let dir = tempfile::tempdir().unwrap();
        let exe = dir.path().join("cmd");
        make_executable(&exe);
        let pathenv = pathenv_of(&[dir.path()]);
        assert_eq!(get_command_path(os("cmd"), &pathenv), Some(exe));
    }

    #[test]
    fn get_command_path_skips_directory_with_same_name() {
        let first = tempfile::tempdir().unwrap();
        let second = tempfile::tempdir().unwrap();
        // A directory named like the command in an earlier PATH entry must
        // not shadow the real executable in a later entry.
        std::fs::create_dir(first.path().join("cmd")).unwrap();
        let exe = second.path().join("cmd");
        make_executable(&exe);
        let pathenv = pathenv_of(&[first.path(), second.path()]);
        assert_eq!(get_command_path(os("cmd"), &pathenv), Some(exe));
    }

    #[test]
    fn get_command_path_no_match_when_only_directory() {
        let dir = tempfile::tempdir().unwrap();
        std::fs::create_dir(dir.path().join("cmd")).unwrap();
        let pathenv = pathenv_of(&[dir.path()]);
        assert_eq!(get_command_path(os("cmd"), &pathenv), None);
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
