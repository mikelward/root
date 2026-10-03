#![deny(unsafe_code)]

use std::ffi::OsString;
use std::os::unix::ffi::OsStringExt;
use std::os::unix::fs::PermissionsExt;
use std::process::Command;

fn root_bin() -> &'static str {
    env!("CARGO_BIN_EXE_root")
}

fn running_as_root() -> bool {
    nix::unistd::Uid::current().is_root()
}

fn in_group_zero() -> bool {
    nix::unistd::getgid().as_raw() == 0 || supplementary_groups().contains(&0)
}

#[cfg(not(target_vendor = "apple"))]
fn supplementary_groups() -> Vec<u32> {
    nix::unistd::getgroups()
        .expect("getgroups failed")
        .iter()
        .map(|g| g.as_raw())
        .collect()
}

/// `nix` has no `getgroups` on Apple targets, and this file allows no
/// unsafe code, so ask `id` instead.
#[cfg(target_vendor = "apple")]
fn supplementary_groups() -> Vec<u32> {
    let out = Command::new("id").arg("-G").output().expect("failed to run id");
    assert!(out.status.success(), "id -G failed: {out:?}");
    String::from_utf8_lossy(&out.stdout)
        .split_whitespace()
        .map(|g| g.parse().expect("id -G printed a non-numeric group"))
        .collect()
}

/// The permission check runs before command resolution, so callers outside
/// group 0 always get PERMISSION_DENIED (123) instead of a resolution error.
fn expected_code(code_when_permitted: i32) -> i32 {
    if in_group_zero() {
        code_when_permitted
    } else {
        123
    }
}

#[test]
fn no_args_prints_usage_and_exits_122() {
    let out = Command::new(root_bin())
        .env_clear()
        .env("PATH", "/usr/bin:/bin")
        .output()
        .expect("failed to run root");
    assert_eq!(out.status.code(), Some(122));
    let stderr = String::from_utf8_lossy(&out.stderr);
    assert!(
        stderr.contains("Usage: root"),
        "stderr was: {stderr}"
    );
}

#[test]
fn unknown_short_option_exits_122() {
    let out = Command::new(root_bin())
        .arg("-x")
        .env("PATH", "/usr/bin:/bin")
        .output()
        .expect("failed to run root");
    assert_eq!(out.status.code(), Some(122));
}

#[test]
fn unknown_long_option_exits_122() {
    let out = Command::new(root_bin())
        .arg("--bogus")
        .env("PATH", "/usr/bin:/bin")
        .output()
        .expect("failed to run root");
    assert_eq!(out.status.code(), Some(122));
}

#[test]
fn nonexistent_qualified_path_exits_127() {
    let out = Command::new(root_bin())
        .arg("/nonexistent/definitely-not-here")
        .env("PATH", "/usr/bin:/bin")
        .output()
        .expect("failed to run root");
    assert_eq!(out.status.code(), Some(expected_code(127)));
}

#[test]
fn relative_path_disallowed_exits_125() {
    // Create a temp dir with an executable. Put `.` first in PATH so the
    // unqualified lookup finds it via a relative entry.
    let dir = tempfile::tempdir().unwrap();
    let cmd_path = dir.path().join("fakecmd");
    std::fs::write(&cmd_path, "#!/bin/sh\nexit 0\n").unwrap();
    let mut perms = std::fs::metadata(&cmd_path).unwrap().permissions();
    perms.set_mode(0o755);
    std::fs::set_permissions(&cmd_path, perms).unwrap();

    let out = Command::new(root_bin())
        .arg("fakecmd")
        .env("PATH", ".:/usr/bin:/bin")
        .current_dir(dir.path())
        .output()
        .expect("failed to run root");
    assert_eq!(
        out.status.code(),
        Some(expected_code(125)),
        "stderr: {}",
        String::from_utf8_lossy(&out.stderr)
    );
    if in_group_zero() {
        let stderr = String::from_utf8_lossy(&out.stderr);
        assert!(
            stderr.contains("potentially unsafe"),
            "stderr was: {stderr}"
        );
    }
}

#[test]
fn unqualified_command_not_found_exits_127() {
    // No `.` in PATH, so a missing command returns COMMAND_NOT_FOUND.
    let out = Command::new(root_bin())
        .arg("definitely-not-a-real-command-xyzzy")
        .env("PATH", "/usr/bin:/bin")
        .output()
        .expect("failed to run root");
    assert_eq!(out.status.code(), Some(expected_code(127)));
}

#[test]
fn non_utf8_qualified_path_does_not_panic() {
    // Path is "/nonexistent/" + 0xFF — not valid UTF-8, but valid Unix argv.
    // The previous String-based implementation would panic on args() before
    // we got anywhere; with OsString-based parsing we should fail cleanly with
    // COMMAND_NOT_FOUND.
    let mut bytes = b"/nonexistent/".to_vec();
    bytes.push(0xFF);
    let bad = OsString::from_vec(bytes);
    let out = Command::new(root_bin())
        .arg(&bad)
        .env("PATH", "/usr/bin:/bin")
        .output()
        .expect("failed to run root");
    assert_eq!(
        out.status.code(),
        Some(expected_code(127)),
        "stderr: {}",
        String::from_utf8_lossy(&out.stderr)
    );
}

#[test]
fn not_in_group_zero_exits_123() {
    if running_as_root() {
        eprintln!("skipping: running as root, group check would pass");
        return;
    }
    // Check whether the current user is in group 0. If so, skip.
    if in_group_zero() {
        eprintln!("skipping: user is in group 0");
        return;
    }
    let out = Command::new(root_bin())
        .arg("/bin/true")
        .env("PATH", "/usr/bin:/bin")
        .output()
        .expect("failed to run root");
    assert_eq!(out.status.code(), Some(123));
}

#[test]
fn messages_escape_untrusted_text() {
    // A command name holding a newline, an ESC and a byte that is not
    // UTF-8. Logged raw, it could forge or hide audit lines; `-d` shows the
    // debug line naming it, which comes before the permission check.
    let cmd = OsString::from_vec(b"bad\ncmd\x1b\xff".to_vec());
    let out = Command::new(root_bin())
        .arg("-d")
        .arg(&cmd)
        .env("PATH", "/usr/bin:/bin")
        .output()
        .expect("failed to run root");
    let stderr = String::from_utf8_lossy(&out.stderr);
    assert!(
        stderr.contains("Command to run is bad\\ncmd\\x1b\\xff\n"),
        "stderr was: {stderr}"
    );
    assert!(!out.stderr.contains(&0x1b), "raw ESC in stderr: {stderr}");
    assert!(!out.stderr.contains(&0xff), "raw 0xff in stderr: {stderr}");
    assert_eq!(out.status.code(), Some(expected_code(127)));
}
