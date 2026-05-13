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
    UnknownOption(String),
}

/// Parse args with POSIX `+` semantics: stop at the first non-option argument.
///
/// `args` is the full argv (including `argv[0]`, the program name); the
/// program name is ignored. The returned `Vec<String>` is the command and its
/// arguments — i.e. the slice of argv beginning at the first non-option,
/// suitable for `execv()`.
pub fn parse(args: &[String]) -> Result<(Options, Vec<String>), ParseError> {
    let mut opts = Options::default();
    let mut i = 1; // skip program name

    while i < args.len() {
        let a = &args[i];

        // Stop at first non-option argument (POSIX `+` semantics).
        if !a.starts_with('-') || a == "-" {
            break;
        }

        // `--` separator: consume it and stop.
        if a == "--" {
            i += 1;
            break;
        }

        if let Some(long) = a.strip_prefix("--") {
            match long {
                "debug" => opts.debug = true,
                "home" => opts.set_home = true,
                "nohome" => opts.set_home = false,
                _ => return Err(ParseError::UnknownOption(a.clone())),
            }
        } else {
            // Short options, possibly combined (e.g. "-dH").
            for ch in a[1..].chars() {
                match ch {
                    'd' => opts.debug = true,
                    'H' => opts.set_home = false,
                    _ => return Err(ParseError::UnknownOption(format!("-{ch}"))),
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

    fn argv(parts: &[&str]) -> Vec<String> {
        std::iter::once("root")
            .chain(parts.iter().copied())
            .map(String::from)
            .collect()
    }

    #[test]
    fn defaults() {
        let (opts, rest) = parse(&argv(&["ls"])).unwrap();
        assert_eq!(opts, Options { set_home: true, debug: false });
        assert_eq!(rest, vec!["ls"]);
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
        assert_eq!(rest, vec!["ls"]);

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
        // `cmd -x` — the -x belongs to cmd.
        let (_opts, rest) = parse(&argv(&["cmd", "-x"])).unwrap();
        assert_eq!(rest, vec!["cmd", "-x"]);
    }

    #[test]
    fn double_dash_separator() {
        let (_opts, rest) = parse(&argv(&["--", "-d"])).unwrap();
        assert_eq!(rest, vec!["-d"]);
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
}
