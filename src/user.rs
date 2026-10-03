use std::ffi::CString;

#[cfg(not(target_vendor = "apple"))]
use nix::unistd::{getgroups, initgroups};
use nix::unistd::{getgid, setgid, setuid, Gid, Group, Uid, User};

#[cfg(target_vendor = "apple")]
use self::libc_groups::{getgroups, initgroups};
use crate::exit_code;
use crate::{debug, error};

/// `getgroups` and `initgroups` straight from libc, with `nix`'s signatures.
///
/// `nix` leaves both out on Apple targets, so this stands in for them there.
/// `getgroups` also builds for tests everywhere, so its buffer handling is
/// checked against `nix` on the platforms where both exist.
#[cfg(any(target_vendor = "apple", test))]
#[allow(unsafe_code)] // libc getgroups/initgroups FFI — buffer sized by the call itself
mod libc_groups {
    use nix::errno::Errno;
    use nix::unistd::Gid;

    /// The calling process's supplementary groups.
    ///
    /// macOS keeps at most 16 groups on the process (`NGROUPS_MAX`) and
    /// resolves any further membership through the directory service, which
    /// this does not see. A user whose group 0 membership lies past those 16
    /// is refused, never wrongly admitted.
    pub fn getgroups() -> nix::Result<Vec<Gid>> {
        // SAFETY: a zero count with a null buffer only asks for the size.
        let count = unsafe { libc::getgroups(0, std::ptr::null_mut()) };
        let len = usize::try_from(count).map_err(|_| Errno::last())?;
        // Fill with a gid no group has, so a slot the second call leaves
        // unwritten can never read as group 0.
        let mut groups = vec![libc::gid_t::MAX; len];
        // SAFETY: `groups` holds exactly `count` entries.
        let got = unsafe { libc::getgroups(count, groups.as_mut_ptr()) };
        let got = usize::try_from(got).map_err(|_| Errno::last())?;
        groups.truncate(got);
        Ok(groups.into_iter().map(Gid::from_raw).collect())
    }

    /// Set the supplementary groups to `user`'s, plus `group`.
    #[cfg(target_vendor = "apple")]
    pub fn initgroups(user: &std::ffi::CStr, group: Gid) -> nix::Result<()> {
        // Apple declares the base group as `int`, not `gid_t`.
        let base = libc::c_int::try_from(group.as_raw()).map_err(|_| Errno::EINVAL)?;
        // SAFETY: `user` is NUL-terminated and outlives the call.
        if unsafe { libc::initgroups(user.as_ptr(), base) } == -1 {
            return Err(Errno::last());
        }
        Ok(())
    }

    #[cfg(all(test, not(target_vendor = "apple")))]
    mod tests {
        #[test]
        fn getgroups_matches_nix() {
            assert_eq!(super::getgroups(), nix::unistd::getgroups());
        }
    }
}

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
