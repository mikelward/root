#ifndef ARGS_H
#define ARGS_H

/*
 * Parsed command-line options.
 *
 * Defaults (set by parse_args): set_home = 1, debug = 0.
 */
struct options {
    int set_home;
    int debug;
};

/*
 * Parse argv with POSIX `+` semantics: option processing stops at the first
 * non-option argument.
 *
 * Only the exact long options --debug, --home, and --nohome are accepted;
 * abbreviations (e.g. --deb) are rejected, matching the Rust parser. Short
 * options -d and -H may be combined (e.g. -dH). A bare "--" terminates option
 * processing and is consumed.
 *
 * On success, *opts is filled in and *argsp is set to the command-and-arguments
 * slice (argv beginning at the first non-option), then 0 is returned. Because
 * argv is NULL-terminated, (*argsp)[0] is NULL when no command was given.
 *
 * On an unknown or abbreviated option, -1 is returned and *argsp is left
 * unchanged.
 */
int parse_args(int argc, const char *const *argv,
               struct options *opts, const char *const **argsp);

#endif
/* vim: set ts=4 sw=4 tw=0 et:*/
