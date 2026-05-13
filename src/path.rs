use std::path::{Path, PathBuf};

use nix::unistd::{access, AccessFlags};

pub const DIRSEP: char = '/';
pub const PATHENVSEP: char = ':';

pub fn is_absolute_path(path: &str) -> bool {
    path.starts_with(DIRSEP)
}

pub fn is_qualified_path(path: &str) -> bool {
    path.contains(DIRSEP)
}

#[allow(dead_code)]
pub fn is_unqualified_path(path: &str) -> bool {
    !path.contains(DIRSEP)
}

/// Iterate over each entry of a PATH-style environment value.
///
/// Empty entries are reported as `.` (POSIX semantics).
pub fn pathenv_each<F: FnMut(&str)>(pathenv: &str, mut func: F) {
    for entry in pathenv.split(PATHENVSEP) {
        let dir = if entry.is_empty() { "." } else { entry };
        func(dir);
    }
}

/// Search `pathenv` for `command`. Return the first directory entry concatenated
/// with `command` that is executable (`X_OK`). Returns `None` if no match.
///
/// The returned path may be relative if the matching PATH entry was relative;
/// the caller is responsible for verifying safety via `is_absolute_path`.
pub fn get_command_path(command: &str, pathenv: &str) -> Option<PathBuf> {
    for entry in pathenv.split(PATHENVSEP) {
        let dir = if entry.is_empty() { "." } else { entry };
        let candidate = Path::new(dir).join(command);
        if access(&candidate, AccessFlags::X_OK).is_ok() {
            return Some(candidate);
        }
    }
    None
}

#[cfg(test)]
mod tests {
    use super::*;

    fn collect(pathenv: &str) -> Vec<String> {
        let mut out = Vec::new();
        pathenv_each(pathenv, |d| out.push(d.to_string()));
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
        assert!(is_absolute_path("/bin/ls"));
        assert!(is_absolute_path("/"));
        assert!(!is_absolute_path("bin/ls"));
        assert!(!is_absolute_path("./ls"));
        assert!(!is_absolute_path(""));
    }

    #[test]
    fn qualified_paths() {
        assert!(is_qualified_path("/bin/ls"));
        assert!(is_qualified_path("./ls"));
        assert!(is_qualified_path("../bin/ls"));
        assert!(!is_qualified_path("ls"));
        assert!(!is_qualified_path(""));
    }

    #[test]
    fn unqualified_paths() {
        assert!(is_unqualified_path("ls"));
        assert!(!is_unqualified_path("/bin/ls"));
        assert!(!is_unqualified_path("./ls"));
        assert!(is_unqualified_path(""));
    }
}
