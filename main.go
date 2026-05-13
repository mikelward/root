package main

import (
	"flag"
	"log/syslog"
	"os"
	"path/filepath"
	"syscall"
)

const (
	exitProgrammerError        = 121
	exitInvalidUsage           = 122
	exitPermissionDenied       = 123
	exitSystemError            = 124
	exitRelativePathDisallowed = 125
	exitErrorExecutingCommand  = 126
	exitCommandNotFound        = 127
)

func main() {
	initLog("root")

	absoluteCommand, args := processArgs(os.Args)

	ensurePermitted()

	// Log before becoming root so the syslog prefix carries the caller's
	// identity.
	infof("Running %s", absoluteCommand)

	becomeRoot()

	runCommand(absoluteCommand, args)
}

// setHome is package-level so flag callbacks can mutate it during parsing.
// Default true mirrors the C version.
var setHome = true

func processArgs(argv []string) (string, []string) {
	fs := flag.NewFlagSet(progName, flag.ContinueOnError)
	fs.Usage = usage

	debugFn := func(string) error { setLogLevel(syslog.LOG_DEBUG); return nil }
	homeFn := func(string) error { setHome = true; return nil }
	nohomeFn := func(string) error { setHome = false; return nil }

	fs.BoolFunc("d", "enable debug logging", debugFn)
	fs.BoolFunc("debug", "enable debug logging", debugFn)
	fs.BoolFunc("H", "do not set HOME", nohomeFn)
	fs.BoolFunc("nohome", "do not set HOME", nohomeFn)
	fs.BoolFunc("home", "set HOME to root's home directory (default)", homeFn)

	if err := fs.Parse(argv[1:]); err != nil {
		os.Exit(exitInvalidUsage)
	}

	rest := fs.Args()
	if len(rest) < 1 {
		usage()
		os.Exit(exitInvalidUsage)
	}
	command := rest[0]
	if command == "" {
		errorf("Command is empty")
		os.Exit(exitInvalidUsage)
	}

	debugf("Command to run is %s", command)
	return resolveCommand(command), rest
}

func resolveCommand(command string) string {
	if isQualifiedPath(command) {
		return absoluteOrDie(command)
	}
	pathenv := os.Getenv("PATH")
	if pathenv == "" {
		errorf("Cannot get PATH environment variable")
		os.Exit(exitSystemError)
	}
	debugf("Searching for command in PATH=%s", pathenv)
	found := findInPath(command, pathenv)
	if found == "" {
		errorf("Cannot find %s in PATH", command)
		os.Exit(exitCommandNotFound)
	}
	if !isAbsolutePath(found) {
		errorf("Attempt to run relative PATH command %s", found)
		printErr("You tried to run %s, but this would run %s\n",
			command, absoluteOrDie(found))
		printErr("This has been prevented because it is potentially unsafe\n")
		printErr("Consider removing the following entries from your PATH:")
		printUnsafePathEntries(pathenv)
		printErr("Or run the command using an absolute path\n")
		printErr("Run \"man root\" for more details\n")
		os.Exit(exitRelativePathDisallowed)
	}
	return absoluteOrDie(found)
}

func absoluteOrDie(p string) string {
	resolved, err := filepath.EvalSymlinks(p)
	if err != nil {
		errorf("Cannot determine real path to %s: %v", p, err)
		os.Exit(exitCommandNotFound)
	}
	abs, err := filepath.Abs(resolved)
	if err != nil {
		errorf("Cannot determine real path to %s: %v", p, err)
		os.Exit(exitCommandNotFound)
	}
	return abs
}

func printUnsafePathEntries(pathenv string) {
	for _, entry := range pathEntries(pathenv) {
		if !isAbsolutePath(entry) {
			printErr(" \"%s\"", entry)
		}
	}
	printErr("\n")
}

func ensurePermitted() {
	if inGroup(rootGID) {
		return
	}
	if name := groupName(rootGID); name != "" {
		errorf("You must be in the %s group to run root", name)
	} else {
		errorf("You must be in group %d to run root", rootGID)
	}
	os.Exit(exitPermissionDenied)
}

func becomeRoot() {
	if setHome {
		if !setHomeDir(rootUID) {
			errorf("Cannot set HOME directory")
			os.Exit(exitSystemError)
		}
	}
	if !setupGroups(rootUID) {
		errorf("Cannot set up groups")
		os.Exit(exitSystemError)
	}
	if !becomeUser(rootUID) {
		errorf("Cannot become root")
		os.Exit(exitSystemError)
	}
}

func runCommand(absolute string, args []string) {
	if err := syscall.Exec(absolute, args, os.Environ()); err != nil {
		errorf("Cannot exec '%s': %v", absolute, err)
		os.Exit(exitErrorExecutingCommand)
	}
}

func usage() {
	printErr("Usage: root [-d | --debug] [-H | --nohome | --home] <command> [<argument>]...\n")
}
