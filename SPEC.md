# root - Specification

## Overview

`root` is a minimal, security-focused alternative to `sudo` that allows members
of group 0 (typically `root` or `wheel`) to run commands as the root user. It
prioritizes predictability over flexibility: unlike `sudo`, it does not modify
`PATH` or other environment variables in surprising ways.

**Author:** Mikel Ward <mikel@mikelward.com>
**License:** Apache License 2.0

## Synopsis

```
root [-H | --nohome | --home] <command> [<argument>]...
```

## Command-Line Options

| Option | Description |
|--------|-------------|
| `-H`, `--nohome` | Do **not** set `HOME`. Leave it unchanged from the calling user's environment. |
| `--home` | Set `HOME` to the target user's (root's) home directory. This is the **default**. Useful to override a previous `-H` in a shell alias. |

Option parsing uses POSIX `+` prefix to stop at the first non-option argument
(i.e. options must come before the command).

## Execution Flow

1. **Initialize logging** - Open syslog with facility `LOG_AUTHPRIV` and flags
   `LOG_CONS | LOG_PID`.

2. **Parse arguments** - Process `-H`/`--nohome`/`--home` flags via
   `getopt_long()`. The remaining arguments are the command and its arguments.

3. **Resolve command** - Determine the absolute path of the command to execute
   (see [Command Resolution](#command-resolution) below).

4. **Check permissions** - Verify the calling user is a member of group 0 (see
   [Permission Model](#permission-model)).

5. **Log the action** - Log the command being run (at `LOG_INFO` level) to
   syslog and stderr. This happens *before* becoming root so the log includes
   the calling user's identity.

6. **Become root:**
   - Optionally set `HOME` to root's home directory (default: yes).
   - Set GID to root's primary group via `setgid()`.
   - Initialize supplementary groups via `initgroups()`.
   - Set UID to 0 via `setuid()`. The program must be installed setuid root
     for this to work (the `setuid()` call promotes `euid=0` to `ruid=0`).

7. **Execute the command** - Call `execv()` (never `execvp()`) with the
   resolved absolute path. The original `argv[0]` (the command name as typed)
   is preserved. On success, `execv` does not return.

## Command Resolution

Commands are resolved differently depending on whether they contain a slash.

### Qualified paths (contain `/`)

Examples: `/bin/ls`, `./mycommand`, `../bin/test`

- Allowed without any PATH lookup.
- Converted to an absolute path via `realpath()`.

### Unqualified paths (no `/`)

Examples: `ls`, `cat`, `grep`

- Looked up in the `PATH` environment variable.
- The first matching file (checked via `access(path, F_OK)`) is used.
- The resolved path **must be absolute**. If the match came from a relative
  `PATH` entry (e.g. `.`, `""`, or `bin` instead of `/bin`), the command is
  **rejected** (see [PATH Safety](#path-safety)).
- After the PATH lookup, the result is further resolved to an absolute path via
  `realpath()`.

### PATH Safety

When an unqualified command is found via PATH, the program checks that the
resulting path is absolute (starts with `/`). This prevents attacks where:

- `PATH` contains `.` or `""` at any position, and a malicious binary exists in
  the current directory.
- `PATH` contains a relative directory like `bin` instead of `/bin`.

If the check fails:

- The attempt is logged as an error.
- The user is shown what command *would* have been run.
- Unsafe PATH entries are listed.
- The program exits with code 125.

**Examples:**

| PATH | Command | CWD has `./ls`? | Result |
|------|---------|-----------------|--------|
| `/usr/bin:/bin` | `ls` | n/a | Allowed - `/usr/bin/ls` |
| `/bin:.` | `ls` | yes | Allowed - `/bin/ls` (found before `.`) |
| `.:/bin` | `ls` | yes | **Blocked** - found via `.` first |
| `/bin:.` | `sl` | yes (`./sl`) | **Blocked** - only found via `.` |

## Permission Model

- Only users who are members of group 0 (GID 0) may use `root`.
- Checked by examining the calling process's real GID and supplementary groups
  via `getgid()` and `getgroups()`.
- No configuration files, no per-command restrictions, no passwords.
- To grant access: `usermod -a -G 0 <user>`

## Environment Handling

- **`HOME`**: Set to root's home directory by default. Suppressed with `-H`.
- **`PATH`**: Not modified (unlike `sudo`).
- **All other variables**: Not modified. The calling user's environment is
  preserved.

## Logging

### Syslog

All log messages are sent to syslog with facility `LOG_AUTHPRIV`:

- **`LOG_INFO`**: Command being executed (includes calling username).
- **`LOG_ERR`**: Errors (permission denied, command not found, etc.).
- **`LOG_DEBUG`**: Debug information (PATH searches, command resolution).

The calling user's username is included in syslog messages. Percent characters
(`%`) in usernames are escaped to `%%` to prevent format string attacks via
`syslog()`.

### Stderr

Messages are also written to stderr if their priority is at or above the
threshold (`LOG_NOTICE` by default, meaning `LOG_ERR` and above are shown).

The `print()` function writes directly to stderr without going through syslog
(used for user-facing messages like PATH safety warnings).

## Exit Codes

| Code | Name | Meaning |
|------|------|---------|
| *N* | *(success)* | The executed command's own exit status |
| 121 | `PROGRAMMER_ERROR` | Internal error (NULL pointer, etc.) |
| 122 | `INVALID_USAGE` | No command specified, or invalid options |
| 123 | `PERMISSION_DENIED` | User is not in group 0 |
| 124 | `SYSTEM_ERROR` | System call failure (`setuid`, `getpwuid`, etc.) |
| 125 | `RELATIVE_PATH_DISALLOWED` | Command found via relative PATH entry |
| 126 | `ERROR_EXECUTING_COMMAND` | `execv()` failed |
| 127 | `COMMAND_NOT_FOUND` | Command not found in PATH / `realpath()` failed |

These mirror shell conventions (126 = command not executable, 127 = command not
found).

## Portability

`root` is written in standard C99 and targets POSIX systems. It builds and runs
on:

- **Linux** (glibc, musl)
- **FreeBSD** / **OpenBSD** / **NetBSD**
- **macOS** (Darwin)

### Portability notes

- The `install` target accepts an `INSTALL_GROUP` variable (default: `root`)
  to accommodate systems where group 0 is named `wheel` (BSD, macOS):
  `make install INSTALL_GROUP=wheel`.

## Installation

The binary must be installed **setuid root** for privilege escalation to work:

```
install -o root -g root -m 4755 root /usr/local/bin/root
```

On BSD and macOS, use group `wheel`:

```
make install INSTALL_GROUP=wheel
```

## Shell Alias Tip

To allow shell aliases to be expanded after `root`, add to your shell config:

```bash
alias root='root '   # trailing space enables alias expansion
```

## Files

| File | Purpose |
|------|---------|
| `root.c` | Main entry point, argument parsing, command resolution, execution |
| `root.h` | Constants: program name, UIDs/GIDs, exit codes |
| `user.c` | Group membership checks, UID/GID switching, HOME directory setting |
| `user.h` | User management function declarations |
| `path.c` | PATH searching, path classification (qualified/absolute/unqualified) |
| `path.h` | Path utility declarations and constants (`DIRSEP`, `PATHENVSEP`) |
| `logging.c` | Syslog and stderr logging, format string escaping |
| `logging.h` | Logging function declarations |
| `root.1` | Man page |

## Design Principles

1. **Simplicity over flexibility** - No config files, no per-command ACLs, no
   password prompts. Access is controlled solely by group membership.

2. **Predictable environment** - The user's environment is preserved as-is
   (except optionally `HOME`). No surprising `PATH` resets.

3. **Defense against PATH attacks** - Unqualified commands found via relative
   PATH entries are blocked, with clear error messages.

4. **Auditability** - All invocations are logged to syslog with the calling
   user's identity.

5. **Minimal attack surface** - Small C codebase, no external dependencies
   beyond libc, strict compiler warnings (`-Wall -Werror`).

6. **Portability** - Uses POSIX-standard C99 and avoids GNU-specific extensions
   so the code builds and runs on Linux, BSD, and macOS.
