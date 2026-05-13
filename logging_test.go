package main

import (
	"bytes"
	"io"
	"log/syslog"
	"os"
	"testing"
)

func captureStderr(t *testing.T, fn func()) string {
	t.Helper()
	orig := os.Stderr
	r, w, err := os.Pipe()
	if err != nil {
		t.Fatalf("pipe: %v", err)
	}
	os.Stderr = w
	done := make(chan []byte)
	go func() {
		var buf bytes.Buffer
		_, _ = io.Copy(&buf, r)
		done <- buf.Bytes()
	}()
	fn()
	_ = w.Close()
	os.Stderr = orig
	return string(<-done)
}

func TestDefaultLogThreshold(t *testing.T) {
	if logThreshold != syslog.LOG_ERR {
		t.Fatalf("default logThreshold = %d, want %d", logThreshold, syslog.LOG_ERR)
	}
}

func TestSetLogLevel(t *testing.T) {
	orig := logThreshold
	t.Cleanup(func() { logThreshold = orig })

	setLogLevel(syslog.LOG_DEBUG)
	if logThreshold != syslog.LOG_DEBUG {
		t.Fatalf("after setLogLevel(LOG_DEBUG), logThreshold = %d", logThreshold)
	}
}

func TestWriteStderrFormat(t *testing.T) {
	orig := logThreshold
	t.Cleanup(func() { logThreshold = orig })
	logThreshold = syslog.LOG_DEBUG

	out := captureStderr(t, func() {
		writeStderr(syslog.LOG_ERR, "hello world")
	})
	want := "root: hello world\n"
	if out != want {
		t.Fatalf("writeStderr output = %q, want %q", out, want)
	}
}

func TestWriteStderrRespectsThreshold(t *testing.T) {
	orig := logThreshold
	t.Cleanup(func() { logThreshold = orig })
	logThreshold = syslog.LOG_ERR

	out := captureStderr(t, func() {
		writeStderr(syslog.LOG_DEBUG, "should be suppressed")
	})
	if out != "" {
		t.Fatalf("writeStderr emitted %q below threshold; want empty", out)
	}
}
