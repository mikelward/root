use std::os::unix::fs::PermissionsExt;
use std::process::Command;

fn root_bin() -> &'static str {
    env!("CARGO_BIN_EXE_root")
}

fn running_as_root() -> bool {
    nix::unistd::Uid::current().is_root()
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
    // Group check happens AFTER command resolution, so this path is reached
    // even by non-root users.
    let out = Command::new(root_bin())
        .arg("/nonexistent/definitely-not-here")
        .env("PATH", "/usr/bin:/bin")
        .output()
        .expect("failed to run root");
    assert_eq!(out.status.code(), Some(127));
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
        Some(125),
        "stderr: {}",
        String::from_utf8_lossy(&out.stderr)
    );
    let stderr = String::from_utf8_lossy(&out.stderr);
    assert!(
        stderr.contains("potentially unsafe"),
        "stderr was: {stderr}"
    );
}

#[test]
fn unqualified_command_not_found_exits_127() {
    // No `.` in PATH, so a missing command returns COMMAND_NOT_FOUND.
    let out = Command::new(root_bin())
        .arg("definitely-not-a-real-command-xyzzy")
        .env("PATH", "/usr/bin:/bin")
        .output()
        .expect("failed to run root");
    assert_eq!(out.status.code(), Some(127));
}

#[test]
fn not_in_group_zero_exits_123() {
    if running_as_root() {
        eprintln!("skipping: running as root, group check would pass");
        return;
    }
    // Check whether the current user is in group 0. If so, skip.
    let in_group_0 = nix::unistd::getgroups()
        .map(|gs| gs.iter().any(|g| g.as_raw() == 0))
        .unwrap_or(false)
        || nix::unistd::getgid().as_raw() == 0;
    if in_group_0 {
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
