package main

import (
	"fmt"
	"log/syslog"
	"os"
	"os/user"
	"strconv"
)

var (
	logThreshold = syslog.LOG_ERR
	sysLogger    *syslog.Writer
	progName     = "root"
)

func initLog(name string) {
	progName = name
	w, err := syslog.New(syslog.LOG_AUTHPRIV|syslog.LOG_INFO, name)
	if err == nil {
		sysLogger = w
	}
}

func setLogLevel(p syslog.Priority) {
	logThreshold = p
}

func writeSyslog(priority syslog.Priority, msg string) {
	if sysLogger == nil {
		return
	}
	prefixed := getUsername(os.Getuid()) + ": " + msg
	switch priority {
	case syslog.LOG_DEBUG:
		_ = sysLogger.Debug(prefixed)
	case syslog.LOG_INFO:
		_ = sysLogger.Info(prefixed)
	case syslog.LOG_ERR:
		_ = sysLogger.Err(prefixed)
	default:
		_ = sysLogger.Notice(prefixed)
	}
}

func writeStderr(priority syslog.Priority, msg string) {
	if priority > logThreshold {
		return
	}
	fmt.Fprintf(os.Stderr, "%s: %s\n", progName, msg)
}

func logf(priority syslog.Priority, format string, args ...any) {
	msg := fmt.Sprintf(format, args...)
	writeSyslog(priority, msg)
	writeStderr(priority, msg)
}

func debugf(format string, args ...any) { logf(syslog.LOG_DEBUG, format, args...) }
func infof(format string, args ...any)  { logf(syslog.LOG_INFO, format, args...) }
func errorf(format string, args ...any) { logf(syslog.LOG_ERR, format, args...) }

// printErr writes to stderr without prefix or trailing newline.
func printErr(format string, args ...any) {
	fmt.Fprintf(os.Stderr, format, args...)
}

func getUsername(uid int) string {
	u, err := user.LookupId(strconv.Itoa(uid))
	if err != nil {
		return "Unknown user"
	}
	return u.Username
}
