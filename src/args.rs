use std::ffi::OsString;
use std::os::unix::ffi::{OsStrExt, OsStringExt};

#[derive(Debug, PartialEq, Eq)]
pub struct Options {
    pub set_home: bool,
    pub debug: bool,
}

impl Default for Options {
    fn default() -> Self {
        Self {
            set_home: true,
            debug: false,
        }
    }
}

#[derive(Debug, PartialEq, Eq)]
pub enum ParseError {
    UnknownOption(OsString),
}

/// Parse args with POSIX `+` semantics: stop at the first non-option argument.
///
/// `args` is the full argv (including `argv[0]`, the program name); the
/// program name is ignored. The returned `Vec<OsString>` is the command and
/// its arguments — i.e. the slice of argv beginning at the first non-option,
/// suitable for `execv()`. Argv bytes are preserved verbatim so non-UTF-8
/// command names and arguments pass through unchanged.
pub fn parse(args: &[OsString]) -> Result<(Options, Vec<OsString>), ParseError> {
    let mut opts = Options::default();
    let mut i = 1; // skip program name

    while i < args.len() {
        let a = &args[i];
        let bytes = a.as_bytes();

        // Stop at first non-option argument (POSIX `+` semantics).
        if bytes.is_empty() || bytes[0] != b'-' || bytes == b"-" {
            break;
        }

        // `--` separator: consume it and stop.
        if bytes == b"--" {
            i += 1;
            break;
        }

        if let Some(long) = bytes.strip_prefix(b"--") {
            match long {
                b"debug" => opts.debug = true,
                b"home" => opts.set_home = true,
                b"nohome" => opts.set_home = false,
                _ => return Err(ParseError::UnknownOption(a.clone())),
            }
        } else {
            // Short options, possibly combined (e.g. "-dH").
            for &c in &bytes[1..] {
                match c {
                    b'd' => opts.debug = true,
                    b'H' => opts.set_home = false,
                    _ => {
                        return Err(ParseError::UnknownOption(OsString::from_vec(vec![
                            b'-', c,
                        ])))
                    }
                }
            }
        }

        i += 1;
    }

    Ok((opts, args[i..].to_vec()))
}

#[cfg(test)]
mod tests {
    use super::*;

    fn argv(parts: &[&str]) -> Vec<OsString> {
        std::iter::once("root")
            .chain(parts.iter().copied())
            .map(OsString::from)
            .collect()
    }

    fn rest_strs(rest: &[OsString]) -> Vec<&str> {
        rest.iter().map(|s| s.to_str().unwrap()).collect()
    }

    #[test]
    fn defaults() {
        let (opts, rest) = parse(&argv(&["ls"])).unwrap();
        assert_eq!(opts, Options { set_home: true, debug: false });
        assert_eq!(rest_strs(&rest), vec!["ls"]);
    }

    #[test]
    fn debug_long_and_short() {
        let (opts, _) = parse(&argv(&["-d", "ls"])).unwrap();
        assert!(opts.debug);
        let (opts, _) = parse(&argv(&["--debug", "ls"])).unwrap();
        assert!(opts.debug);
    }

    #[test]
    fn nohome() {
        let (opts, rest) = parse(&argv(&["-H", "ls"])).unwrap();
        assert!(!opts.set_home);
        assert_eq!(rest_strs(&rest), vec!["ls"]);

        let (opts, _) = parse(&argv(&["--nohome", "ls"])).unwrap();
        assert!(!opts.set_home);
    }

    #[test]
    fn home_overrides_nohome() {
        let (opts, _) = parse(&argv(&["-H", "--home", "ls"])).unwrap();
        assert!(opts.set_home);
    }

    #[test]
    fn combined_short_options() {
        let (opts, _) = parse(&argv(&["-dH", "ls"])).unwrap();
        assert!(opts.debug);
        assert!(!opts.set_home);
    }

    #[test]
    fn stops_at_first_non_option() {
        let (_opts, rest) = parse(&argv(&["cmd", "-x"])).unwrap();
        assert_eq!(rest_strs(&rest), vec!["cmd", "-x"]);
    }

    #[test]
    fn double_dash_separator() {
        let (_opts, rest) = parse(&argv(&["--", "-d"])).unwrap();
        assert_eq!(rest_strs(&rest), vec!["-d"]);
    }

    #[test]
    fn unknown_long_option() {
        assert!(matches!(
            parse(&argv(&["--bogus"])),
            Err(ParseError::UnknownOption(_))
        ));
    }

    #[test]
    fn unknown_short_option() {
        assert!(matches!(
            parse(&argv(&["-x"])),
            Err(ParseError::UnknownOption(_))
        ));
    }

    #[test]
    fn no_command() {
        let (_opts, rest) = parse(&argv(&["-d"])).unwrap();
        assert!(rest.is_empty());
    }

    #[test]
    fn non_utf8_command_passes_through() {
        // argv[1] = "./" + 0xFF — invalid UTF-8 but valid Unix argv.
        let progname = OsString::from("root");
        let bad = OsString::from_vec(vec![b'.', b'/', 0xFF]);
        let args = vec![progname, bad.clone()];
        let (_opts, rest) = parse(&args).unwrap();
        assert_eq!(rest, vec![bad]);
    }

    #[test]
    fn non_utf8_argument_passes_through() {
        let progname = OsString::from("root");
        let cmd = OsString::from("echo");
        let bad = OsString::from_vec(vec![0xFE, 0xFD]);
        let args = vec![progname, cmd.clone(), bad.clone()];
        let (_opts, rest) = parse(&args).unwrap();
        assert_eq!(rest, vec![cmd, bad]);
    }
}
