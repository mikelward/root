package main

import (
	"os"
	"os/user"
	"strconv"
	"syscall"
)

const (
	rootUID = 0
	rootGID = 0
)

// groupName returns the textual name of gid, or "" if it cannot be resolved.
func groupName(gid int) string {
	g, err := user.LookupGroupId(strconv.Itoa(gid))
	if err != nil {
		return ""
	}
	return g.Name
}

// inGroup reports whether the calling process is a member of gid, either via
// its real GID or its supplementary group list.
func inGroup(gid int) bool {
	if os.Getgid() == gid {
		return true
	}
	groups, err := os.Getgroups()
	if err != nil {
		errorf("Cannot get group list: %v", err)
		os.Exit(exitSystemError)
	}
	for _, g := range groups {
		if g == gid {
			return true
		}
	}
	return false
}

// setHomeDir sets $HOME to uid's home directory.
func setHomeDir(uid int) bool {
	u, err := user.LookupId(strconv.Itoa(uid))
	if err != nil {
		errorf("Cannot get passwd info for uid %d: %v", uid, err)
		os.Exit(exitSystemError)
	}
	return os.Setenv("HOME", u.HomeDir) == nil
}

// setupGroups sets the GID and supplementary groups for uid.
//
// Order mirrors the C version: setgid first, then initgroups-equivalent
// via syscall.Setgroups. On Linux (Go 1.16+), syscall.Setgid/Setgroups use
// AllThreadsSyscall, so credentials apply to every runtime thread.
func setupGroups(uid int) bool {
	u, err := user.LookupId(strconv.Itoa(uid))
	if err != nil {
		errorf("Cannot get passwd info for uid %d: %v", uid, err)
		os.Exit(exitSystemError)
	}
	primaryGID, err := strconv.Atoi(u.Gid)
	if err != nil {
		errorf("Cannot parse primary gid %q for uid %d: %v", u.Gid, uid, err)
		os.Exit(exitSystemError)
	}
	if err := syscall.Setgid(primaryGID); err != nil {
		errorf("Cannot setgid %d: %v", primaryGID, err)
		os.Exit(exitSystemError)
	}
	gidStrs, err := u.GroupIds()
	if err != nil {
		errorf("Cannot get supplementary groups for %s: %v", u.Username, err)
		os.Exit(exitSystemError)
	}
	gids := make([]int, 0, len(gidStrs))
	for _, s := range gidStrs {
		g, err := strconv.Atoi(s)
		if err != nil {
			errorf("Cannot parse supplementary gid %q: %v", s, err)
			os.Exit(exitSystemError)
		}
		gids = append(gids, g)
	}
	if err := syscall.Setgroups(gids); err != nil {
		errorf("Cannot setgroups for %s: %v", u.Username, err)
		os.Exit(exitSystemError)
	}
	return true
}

// becomeUser switches the process credentials to uid. Currently only uid 0
// is supported.
func becomeUser(uid int) bool {
	if uid != rootUID {
		errorf("Becoming non-root user has not been tested")
		return false
	}
	if err := syscall.Setuid(uid); err != nil {
		errorf("Cannot setuid %d: %v", uid, err)
		os.Exit(exitSystemError)
	}
	return true
}
