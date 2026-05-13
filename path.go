package main

import (
	"strings"
	"syscall"
)

const (
	dirSep     = '/'
	pathEnvSep = ':'

	// POSIX access() mode bit for execute permission. The stdlib syscall
	// package does not export X_OK on all platforms, so we define it here.
	xOK = 0x1
)

func isAbsolutePath(p string) bool {
	return len(p) > 0 && p[0] == dirSep
}

func isQualifiedPath(p string) bool {
	return strings.ContainsRune(p, dirSep)
}

func isUnqualifiedPath(p string) bool {
	return p != "" && !strings.ContainsRune(p, dirSep)
}

// pathEntries splits PATH on ':' and maps empty entries to ".",
// matching POSIX semantics and the C pathenv_each behavior.
func pathEntries(pathenv string) []string {
	parts := strings.Split(pathenv, string(pathEnvSep))
	for i, p := range parts {
		if p == "" {
			parts[i] = "."
		}
	}
	return parts
}

// findInPath returns the first directory/command join in pathenv for which
// the resulting file is executable (X_OK). The returned path may be relative
// when the matching PATH entry was relative; callers should verify safety.
// Returns "" if no match is found.
func findInPath(command, pathenv string) string {
	for _, dir := range pathEntries(pathenv) {
		var candidate string
		if strings.HasSuffix(dir, string(dirSep)) {
			candidate = dir + command
		} else {
			candidate = dir + string(dirSep) + command
		}
		if syscall.Access(candidate, xOK) == nil {
			return candidate
		}
	}
	return ""
}
