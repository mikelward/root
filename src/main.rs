#![deny(unsafe_code)]

use std::ffi::CString;
use std::path::{Path, PathBuf};

use nix::unistd::execv;

mod args;
mod exit_code;
pub mod logging;
mod path;
mod user;

const PROGNAME: &str = "root";
const ROOT_UID: u32 = 0;
const ROOT_GID: u32 = 0;

fn main() {
    logging::init(PROGNAME);

    let argv: Vec<String> = std::env::args().collect();

    let (opts, command_args) = match args::parse(&argv) {
        Ok(v) => v,
        Err(args::ParseError::UnknownOption(o)) => {
            error!("Unknown option: {o}");
            usage();
            std::process::exit(exit_code::INVALID_USAGE);
        }
    };

    if opts.debug {
        logging::set_level(logging::LOG_DEBUG);
    }

    if command_args.is_empty() {
        usage();
        std::process::exit(exit_code::INVALID_USAGE);
    }

    let command = &command_args[0];
    if command.is_empty() {
        error!("Command is empty");
        std::process::exit(exit_code::INVALID_USAGE);
    }

    debug!("Command to run is {command}");

    let absolute_command = resolve_command(command);

    ensure_permitted();

    // Log before dropping the calling user's identity so syslog records who ran it.
    info!("Running {}", absolute_command.display());

    become_root(opts.set_home);

    exec_command(&absolute_command, &command_args);
}

fn resolve_command(command: &str) -> PathBuf {
    if path::is_qualified_path(command) {
        get_absolute_command(command)
    } else {
        let resolved = find_and_verify_command(command);
        get_absolute_command(resolved.to_string_lossy().as_ref())
    }
}

fn get_absolute_command(qualified: &str) -> PathBuf {
    match std::fs::canonicalize(qualified) {
        Ok(p) => p,
        Err(e) => {
            error!("Cannot determine real path to {qualified}: {e}");
            std::process::exit(exit_code::COMMAND_NOT_FOUND);
        }
    }
}

fn find_and_verify_command(command: &str) -> PathBuf {
    let pathenv = match std::env::var("PATH") {
        Ok(v) => v,
        Err(_) => {
            error!("Cannot get PATH environment variable");
            std::process::exit(exit_code::SYSTEM_ERROR);
        }
    };

    debug!("Searching for command in PATH={pathenv}");
    let path_command = match path::get_command_path(command, &pathenv) {
        Some(p) => p,
        None => {
            error!("Cannot find {command} in PATH");
            std::process::exit(exit_code::COMMAND_NOT_FOUND);
        }
    };

    let path_str = path_command.to_string_lossy();
    if !path::is_absolute_path(&path_str) {
        error!("Attempt to run relative PATH command {path_str}");
        let absolute = std::fs::canonicalize(&path_command)
            .map(|p| p.to_string_lossy().into_owned())
            .unwrap_or_else(|_| path_str.to_string());
        print_stderr!(
            "You tried to run {command}, but this would run {absolute}\n"
        );
        print_stderr!("This has been prevented because it is potentially unsafe\n");
        print_stderr!("Consider removing the following entries from your PATH:");
        print_unsafe_path_entries(&pathenv);
        print_stderr!("Or run the command using an absolute path\n");
        print_stderr!("Run \"man root\" for more details\n");
        std::process::exit(exit_code::RELATIVE_PATH_DISALLOWED);
    }

    path_command
}

fn print_unsafe_path_entries(pathenv: &str) {
    path::pathenv_each(pathenv, |dir| {
        if !path::is_absolute_path(dir) {
            print_stderr!(" \"{dir}\"");
        }
    });
    print_stderr!("\n");
}

fn ensure_permitted() {
    if !user::in_group(ROOT_GID) {
        match user::get_group_name(ROOT_GID) {
            Some(name) => error!("You must be in the {name} group to run root"),
            None => error!("You must be in group {ROOT_GID} to run root"),
        }
        std::process::exit(exit_code::PERMISSION_DENIED);
    }
}

fn become_root(set_home: bool) {
    if set_home && !user::set_home_dir(ROOT_UID) {
        error!("Cannot set HOME directory");
        std::process::exit(exit_code::SYSTEM_ERROR);
    }
    user::setup_groups(ROOT_UID);
    if !user::become_user(ROOT_UID) {
        error!("Cannot become root");
        std::process::exit(exit_code::SYSTEM_ERROR);
    }
}

fn exec_command(absolute: &Path, argv: &[String]) {
    let c_path = match CString::new(absolute.as_os_str().as_encoded_bytes()) {
        Ok(c) => c,
        Err(_) => {
            error!("Path contains NUL byte");
            std::process::exit(exit_code::ERROR_EXECUTING_COMMAND);
        }
    };

    let c_args: Result<Vec<CString>, _> =
        argv.iter().map(|a| CString::new(a.as_bytes())).collect();
    let c_args = match c_args {
        Ok(v) => v,
        Err(_) => {
            error!("Argument contains NUL byte");
            std::process::exit(exit_code::ERROR_EXECUTING_COMMAND);
        }
    };

    match execv(&c_path, &c_args) {
        Ok(_) => unreachable!("execv returned Ok"),
        Err(e) => {
            error!("Cannot exec '{}': {}", absolute.display(), e);
            std::process::exit(exit_code::ERROR_EXECUTING_COMMAND);
        }
    }
}

fn usage() {
    print_stderr!(
        "Usage: root [-d | --debug] [-H | --nohome | --home] <command> [<argument>]...\n"
    );
}
