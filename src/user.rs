use std::ffi::{CString, OsString};
use std::os::unix::ffi::OsStrExt;

#[cfg(not(target_vendor = "apple"))]
use nix::unistd::{getgroups, initgroups};
use nix::unistd::{getgid, setgid, setuid, Gid, Uid, User};

#[cfg(target_vendor = "apple")]
use self::libc_groups::{getgroups, initgroups};
pub use self::names::user_name;
use self::names::group_name;
use crate::exit_code;
use crate::logging;
use crate::{debug, error};

/// User and group names as the system stores them.
///
/// `nix` builds `User::name` and `Group::name` with `to_string_lossy`, which
/// turns bytes that are not valid UTF-8 into U+FFFD, so the audit log could
/// not tell such names apart and `initgroups` could be handed a name that
/// is not the user's. These call `getpwuid_r` and `getgrgid_r` directly and
/// keep the raw bytes.
#[allow(unsafe_code)] // libc getpwuid_r/getgrgid_r FFI — buffer owned here, result checked
mod names {
    use std::ffi::{CStr, OsStr, OsString};
    use std::os::unix::ffi::OsStrExt;

    use nix::errno::Errno;
    use nix::unistd::{Gid, Uid};

    /// The largest buffer to offer before giving up; real entries are tiny.
    const MAX_BUFFER: usize = 1 << 20;

    /// The name of the user with this uid, or `None` if there is no such user.
    pub fn user_name(uid: Uid) -> nix::Result<Option<OsString>> {
        // SAFETY: an all-zero passwd is valid: null pointers and zero ids.
        let entry: libc::passwd = unsafe { std::mem::zeroed() };
        lookup(
            entry,
            // SAFETY: lookup passes a live entry, a buffer of `len` bytes and
            // a result slot, which is all getpwuid_r writes to.
            |entry, buf, len, found| unsafe {
                libc::getpwuid_r(uid.as_raw(), entry, buf, len, found)
            },
            |entry| entry.pw_name,
        )
    }

    /// The name of the group with this gid, or `None` if there is no such group.
    pub fn group_name(gid: Gid) -> nix::Result<Option<OsString>> {
        // SAFETY: an all-zero group is valid: null pointers and a zero id.
        let entry: libc::group = unsafe { std::mem::zeroed() };
        lookup(
            entry,
            // SAFETY: as in user_name, for getgrgid_r.
            |entry, buf, len, found| unsafe {
                libc::getgrgid_r(gid.as_raw(), entry, buf, len, found)
            },
            |entry| entry.gr_name,
        )
    }

    /// Run a `get*_r` lookup, doubling its buffer while it reports `ERANGE`,
    /// and copy out the name it found.
    fn lookup<E>(
        mut entry: E,
        mut call: impl FnMut(*mut E, *mut libc::c_char, usize, *mut *mut E) -> libc::c_int,
        name: impl Fn(&E) -> *const libc::c_char,
    ) -> nix::Result<Option<OsString>> {
        let mut size = 1024;
        loop {
            let mut buf: Vec<libc::c_char> = vec![0; size];
            let mut found: *mut E = std::ptr::null_mut();
            match call(&mut entry, buf.as_mut_ptr(), buf.len(), &mut found) {
                0 if found.is_null() => return Ok(None),
                0 => {
                    let ptr = name(&entry);
                    if ptr.is_null() {
                        return Ok(Some(OsString::new()));
                    }
                    // SAFETY: on success the name is a NUL-terminated string
                    // in `buf`, which is still alive here.
                    let bytes = unsafe { CStr::from_ptr(ptr) }.to_bytes();
                    return Ok(Some(OsStr::from_bytes(bytes).to_owned()));
                }
                libc::ERANGE if size < MAX_BUFFER => size *= 2,
                errno => return Err(Errno::from_raw(errno)),
            }
        }
    }

    #[cfg(test)]
    mod tests {
        use super::*;

        fn zeroed_passwd() -> libc::passwd {
            // SAFETY: as in user_name.
            unsafe { std::mem::zeroed() }
        }

        fn pw_name(entry: &libc::passwd) -> *const libc::c_char {
            entry.pw_name
        }

        #[test]
        fn lookup_keeps_raw_bytes_and_grows_its_buffer() {
            // "jos" and 0xe9, Latin-1 for "josé": not valid UTF-8, so nix
            // would have handed back "jos\u{fffd}".
            let stored = b"jos\xe9\0";
            let mut calls = 0;
            let found = lookup(
                zeroed_passwd(),
                |entry, buf, len, found| {
                    calls += 1;
                    if len < 4096 {
                        return libc::ERANGE;
                    }
                    // SAFETY: lookup's buffer holds `len` bytes, far more
                    // than `stored`, and `entry` and `found` are live.
                    unsafe {
                        std::ptr::copy_nonoverlapping(stored.as_ptr().cast(), buf, stored.len());
                        (*entry).pw_name = buf;
                        *found = entry;
                    }
                    0
                },
                pw_name,
            );
            assert_eq!(found, Ok(Some(OsStr::from_bytes(b"jos\xe9").to_owned())));
            assert_eq!(calls, 3, "expected tries at 1024, 2048 and 4096 bytes");
        }

        #[test]
        fn lookup_reports_absence_and_errors() {
            assert_eq!(lookup(zeroed_passwd(), |_, _, _, _| 0, pw_name), Ok(None));
            assert_eq!(
                lookup(zeroed_passwd(), |_, _, _, _| libc::EIO, pw_name),
                Err(Errno::EIO)
            );
            // A lookup that never fits gives up at the cap.
            assert_eq!(
                lookup(zeroed_passwd(), |_, _, _, _| libc::ERANGE, pw_name),
                Err(Errno::ERANGE)
            );
        }

        #[test]
        fn real_lookups_agree_with_nix_on_valid_names() {
            let uid = Uid::current();
            let ours = user_name(uid).unwrap().map(|n| n.to_string_lossy().into_owned());
            let theirs = nix::unistd::User::from_uid(uid).unwrap().map(|u| u.name);
            assert_eq!(ours, theirs);

            let gid = Gid::from_raw(0);
            let ours = group_name(gid).unwrap().map(|n| n.to_string_lossy().into_owned());
            let theirs = nix::unistd::Group::from_gid(gid).unwrap().map(|g| g.name);
            assert_eq!(ours, theirs);
        }
    }
}

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

/// The group's name, raw, if it can be looked up. For messages only.
pub fn get_group_name(gid: u32) -> Option<OsString> {
    group_name(Gid::from_raw(gid)).ok().flatten()
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

    // The raw name, not nix's lossily converted `user.name`, so initgroups
    // looks up exactly this user.
    let name = match user_name(user.uid) {
        Ok(Some(name)) => name,
        Ok(None) => {
            error!("Cannot get passwd info for uid {uid}");
            std::process::exit(exit_code::SYSTEM_ERROR);
        }
        Err(e) => {
            error!("Cannot get passwd info for uid {uid}: {e}");
            std::process::exit(exit_code::SYSTEM_ERROR);
        }
    };
    let cname = match CString::new(name.as_bytes()) {
        Ok(c) => c,
        Err(_) => {
            error!("Username for uid {uid} contains NUL");
            std::process::exit(exit_code::SYSTEM_ERROR);
        }
    };

    if let Err(e) = initgroups(&cname, user.gid) {
        error!("Cannot initgroups for {}: {}", logging::escape(&name), e);
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
    debug!("Set HOME to {}", logging::escape(user.dir.as_os_str()));
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
