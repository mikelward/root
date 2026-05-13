package main

import (
	"os"
	"path/filepath"
	"reflect"
	"testing"
)

func TestPathEntries(t *testing.T) {
	cases := []struct {
		in   string
		want []string
	}{
		{"/usr/bin:/bin", []string{"/usr/bin", "/bin"}},
		{"/usr/bin", []string{"/usr/bin"}},
		{":/usr/bin", []string{".", "/usr/bin"}},
		{"/usr/bin:", []string{"/usr/bin", "."}},
		{"/usr/bin::/bin", []string{"/usr/bin", ".", "/bin"}},
		{"::", []string{".", ".", "."}},
	}
	for _, c := range cases {
		got := pathEntries(c.in)
		if !reflect.DeepEqual(got, c.want) {
			t.Errorf("pathEntries(%q) = %v, want %v", c.in, got, c.want)
		}
	}
}

func TestIsAbsolutePath(t *testing.T) {
	cases := []struct {
		in   string
		want bool
	}{
		{"/bin/ls", true},
		{"/", true},
		{"bin/ls", false},
		{"./ls", false},
		{"", false},
	}
	for _, c := range cases {
		if got := isAbsolutePath(c.in); got != c.want {
			t.Errorf("isAbsolutePath(%q) = %v, want %v", c.in, got, c.want)
		}
	}
}

func TestIsQualifiedPath(t *testing.T) {
	cases := []struct {
		in   string
		want bool
	}{
		{"/bin/ls", true},
		{"./ls", true},
		{"../bin/ls", true},
		{"ls", false},
		{"", false},
	}
	for _, c := range cases {
		if got := isQualifiedPath(c.in); got != c.want {
			t.Errorf("isQualifiedPath(%q) = %v, want %v", c.in, got, c.want)
		}
	}
}

func TestIsUnqualifiedPath(t *testing.T) {
	cases := []struct {
		in   string
		want bool
	}{
		{"ls", true},
		{"/bin/ls", false},
		{"./ls", false},
		{"", false},
	}
	for _, c := range cases {
		if got := isUnqualifiedPath(c.in); got != c.want {
			t.Errorf("isUnqualifiedPath(%q) = %v, want %v", c.in, got, c.want)
		}
	}
}

func TestFindInPath(t *testing.T) {
	dir1 := t.TempDir()
	dir2 := t.TempDir()

	mkExec := func(parent, name string) string {
		p := filepath.Join(parent, name)
		if err := os.WriteFile(p, []byte("#!/bin/sh\n"), 0o755); err != nil {
			t.Fatalf("write %s: %v", p, err)
		}
		return p
	}
	mkExec(dir2, "mytool")
	want := mkExec(dir1, "mytool")

	got := findInPath("mytool", dir1+":"+dir2)
	if got != want {
		t.Errorf("findInPath returned %q, want %q (first PATH entry)", got, want)
	}

	if got := findInPath("doesnotexist", dir1+":"+dir2); got != "" {
		t.Errorf("findInPath for missing command returned %q, want empty", got)
	}
}
