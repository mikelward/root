use std::ffi::CString;

use nix::unistd::{
    getgid, getgroups, initgroups, setgid, setuid, Gid, Group, Uid, User,
};

use crate::exit_code;
use crate::{debug, error};

pub fn get_group_name(gid: u32) -> Option<String> {
    match Group::from_gid(Gid::from_raw(gid)) {
        Ok(Some(g)) => Some(g.name),
        _ => None,
    }
}

pub fn in_group(target_gid: u32) -> bool {
    let target = Gid::from_raw(target_gid);

    if getgid() == target {
        return true;
    }

    match getgroups() {
        Ok(groups) => groups.into_iter().any(|g| g == target),
        Err(e) => {
            error!("Cannot get group list: {e}");
            std::process::exit(exit_code::SYSTEM_ERROR);
        }
    }
}

fn target_user(uid: u32) -> User {
    match User::from_uid(Uid::from_raw(uid)) {
        Ok(Some(u)) => u,
        Ok(None) => {
            error!("Cannot get passwd info for uid {uid}");
            std::process::exit(exit_code::SYSTEM_ERROR);
        }
        Err(e) => {
            error!("Cannot get passwd info for uid {uid}: {e}");
            std::process::exit(exit_code::SYSTEM_ERROR);
        }
    }
}

/// Set the target user's primary group and initialize supplementary groups.
///
/// Exits on failure.
pub fn setup_groups(uid: u32) {
    let user = target_user(uid);

    if let Err(e) = setgid(user.gid) {
        error!("Cannot setgid {}: {}", user.gid.as_raw(), e);
        std::process::exit(exit_code::SYSTEM_ERROR);
    }

    let cname = match CString::new(user.name.clone()) {
        Ok(c) => c,
        Err(_) => {
            error!("Username for uid {uid} contains NUL");
            std::process::exit(exit_code::SYSTEM_ERROR);
        }
    };

    if let Err(e) = initgroups(&cname, user.gid) {
        error!("Cannot initgroups for {}: {}", user.name, e);
        std::process::exit(exit_code::SYSTEM_ERROR);
    }
}

/// Set `HOME` to the target user's home directory.
///
/// Exits if `getpwuid` fails. Returns `false` if `setenv` fails.
pub fn set_home_dir(uid: u32) -> bool {
    let user = target_user(uid);
    // `set_var` is safe in a single-threaded program; main has not spawned threads.
    std::env::set_var("HOME", &user.dir);
    debug!("Set HOME to {}", user.dir.display());
    true
}

/// Become the target user via `setuid`.
///
/// Only uid 0 is supported. Exits on `setuid` failure.
pub fn become_user(uid: u32) -> bool {
    if uid != 0 {
        error!("Becoming non-root user has not been tested");
        return false;
    }

    if let Err(e) = setuid(Uid::from_raw(uid)) {
        error!("Cannot setuid {uid}: {e}");
        std::process::exit(exit_code::SYSTEM_ERROR);
    }
    true
}
