/*
 * root
 *
 * simple reimplementation of sudo
 *
 * Mikel Ward <mikel@mikelward.com>
 */

/*
 * XXX realpath(..., NULL) requires _GNU_SOURCE or _XOPEN_SOURCE 700
 */
#define _GNU_SOURCE             /* for realpath(..., NULL) */
#define _BSD_SOURCE             /* strdup(), initgroups(), etc. */

#include <sys/types.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "root.h"
#include "user.h"
#include "logging.h"
#include "path.h"

static void setup_logging();
static void process_args(int argc, const char *const *argv,
                         char **absolute_commandp, const char *const **argsp);
static void get_command_to_run(const char *command, char **absolute_commandp);
static void find_and_verify_command(const char *command, char **path_commandp);
static void get_absolute_command(const char *qualified_command,
                                 char **absolute_command);
static void find_and_verify_command(const char *command, char **path_command);
static int  command_is_safe(const char *path_command);
static void print_unsafe_path_entries(const char *pathenv);
static void ensure_permitted(void);
static void become_root(void);
static void run_command(const char *absolute_command, const char *const *args);
static void usage(void);

int main(int argc, const char *const *argv)
{
    char *absolute_command = NULL;
    const char *const *args = NULL;

    setup_logging();

    process_args(argc, argv, &absolute_command, &args);

    ensure_permitted();

    /*
     * Do this before become_root so we can log the calling username/uid.
     *
     * XXX log the command arguments too?
     */
    info("Running %s", absolute_command);

    become_root();

    run_command(absolute_command, args);

    /* NOT REACHED */
    return 0;
}

void setup_logging()
{
    initlog(PROGNAME);
}

/**
 * Process command line arguments and determine the command to run.
 *
 * We read the command line arguments to figure out what command to run.
 *
 * The first argument (argv[0]) is the command.
 *
 * The command is looked up and converted to an absolute path by
 * get_command_to_run, then stored in *absolute_commandp.
 *
 * The remaining arguments (argv -> *argsp)
 * (including argv[0] due to C/UNIX conventions)
 * are the arguments that will be passed unchanged to execv().
 */
void process_args(int argc, const char *const *argv,
                  char **absolute_commandp, const char *const **argsp)
{
    if (argc < 2) {
        usage();
        exit(ROOT_INVALID_USAGE);
    }

    if (absolute_commandp == NULL) {
        error("process_args: absolute_commandp is NULL");
        exit(ROOT_PROGRAMMER_ERROR);
    }

    if (argsp == NULL) {
        error("process_args: argsp is NULL");
        exit(ROOT_PROGRAMMER_ERROR);
    }

    /* skip over our own program name */
    argv++, argc--;

    const char *command = argv[0];
    if (command == NULL || *command == '\0') {
        error("Command is NULL");
        exit(ROOT_INVALID_USAGE);
    }

    debug("Command to run is %s", command);

    get_command_to_run(command, absolute_commandp);

    *argsp = argv;
}

/**
 * Determine what command to run and do some safety checks.
 *
 * On success, the absolute path of the command to run is stored
 * in *absolute_commandp.
 *
 * On failure, this function calls exit().
 *
 * The implementation of the safety checks are a bit complicated,
 * but the idea is simple.
 *
 * If the command contains a slash, allow it.  This is called a
 * "qualified command".
 *
 * If the command doesn't contain a slash, look it up in PATH
 * and do some safety checks.  This is called an
 * "unqualified command".
 *
 * The safety checks try to prevent an attack where a malicious
 * user placed malicious code in "/tmp/ls", the user had "."
 * at the start of PATH, and the user tried to run "root ls".
 * 
 * We also want to prohibit running a command if it matched "." at the
 * end of PATH, for example if an attacker created "/tmp/sl", then even
 * if "/usr/bin" and "/bin" were at the front of PATH, a simple typo
 * could result in running the wrong command.
 *
 * Examples:
 *  - "/bin/ls" is allowed
 *  - "./ls" is allowed
 *  - "ls" is allowed if PATH="/bin" and "/bin/ls" exists
 *  - "ls" is allowed if PATH="/bin:." and "/bin/ls" exists
 *  - "ls" is prohibited if PATH=".:/bin" and "./ls" exists
 *  - "sl" is prohibited if PATH="/bin:." and "./sl" exists
 *
 */
void get_command_to_run(const char *command, char **absolute_commandp)
{
    if (command == NULL) {
        error("get_command_to_run: command is NULL");
        exit(ROOT_PROGRAMMER_ERROR);
    }
    if (absolute_commandp == NULL) {
        error("get_command_to_run: absolute_commandp is NULL");
        exit(ROOT_PROGRAMMER_ERROR);
    }

    char *absolute_command = NULL;

    if (is_qualified_path(command)) {
        /*
         * path contained a slash,
         * don't need to look it up in PATH
         */
        get_absolute_command(command, &absolute_command);
    }
    else {
        /*
         * path didn't contain a slash,
         * look it up in PATH and make sure it's safe
         */
        char *path_command = NULL;
        find_and_verify_command(command, &path_command);

        get_absolute_command(path_command, &absolute_command);
        free(path_command);
    }
    *absolute_commandp = absolute_command;
}

void get_absolute_command(const char *qualified_command,
                          char **absolute_commandp)
{
    if (qualified_command == NULL) {
        error("get_absolute_command: qualified_command is NULL");
        exit(ROOT_PROGRAMMER_ERROR);
    }
    if (absolute_commandp == NULL) {
        error("get_absolute_command: absolute_commandp is NULL");
        exit(ROOT_PROGRAMMER_ERROR);
    }

    char *absolute_command = realpath(qualified_command, NULL);
    if (absolute_command == NULL) {
        error("Cannot determine real path to %s", qualified_command);
        exit(ROOT_COMMAND_NOT_FOUND);
    }

    *absolute_commandp = absolute_command;
}

void find_and_verify_command(const char *command, char **path_commandp)
{
    if (command == NULL) {
        error("find_and_verify_command: command is NULL");
        exit(ROOT_PROGRAMMER_ERROR);
    }
    if (path_commandp == NULL) {
        error("find_and_verify_command: path_commandp is NULL");
        exit(ROOT_PROGRAMMER_ERROR);
    }

    const char *pathenv = getenv("PATH");
    if (pathenv == NULL) {
        error("Cannot get PATH environment variable");
        exit(ROOT_SYSTEM_ERROR);
    }

    debug("Searching for command in PATH=%s", pathenv);
    char *path_command = get_command_path(command, pathenv);
    if (path_command == NULL) {
        error("Cannot find %s in PATH", command);
        exit(ROOT_COMMAND_NOT_FOUND);
    }

    /*
     * ensure the command is safe
     */
    if (!command_is_safe(path_command)) {
        /*
         * XXX
         * this should only go to the log file
         */
        error("Attempt to run relative PATH command %s", path_command);
        char *absolute_command;
        get_absolute_command(path_command, &absolute_command);
        print("You tried to run %s, but this would run %s",
               command, absolute_command);
        print("This has been prevented because it is potentially unsafe");
        print("Consider removing the following entries from your PATH:");
        print_unsafe_path_entries(pathenv);
        print("Or run the command using an absolute path");
        print("Run \"man root\" for more details");
        exit(ROOT_RELATIVE_PATH_DISALLOWED);
    }

    *path_commandp = path_command;
}

/**
 * Returns 1 (true) if command is regarded as safe.
 * Returns 0 (false) otherwise.
 */
int command_is_safe(const char *path_command)
{
    /*
     * Ensure the path is an absolute PATH to prevent running
     * a malicious program installed by a rogue user in the current
     * directory (or any relative entry in PATH).
     *
     * We could just check for a command starting with a dot, but I also
     * want to handle the case where somebody puts something strange like
     * "bin" in PATH (as opposed to "/bin"), which could also result in
     * running an unintended command.  Ensuring the resultant path is
     * absolute handles both cases.
     */
    return is_absolute_path(path_command);
}

void print_if_unsafe(const char *dir)
{
    if (!command_is_safe(dir)) {
        print("\t\"%s\"", dir);
    }
}

void print_unsafe_path_entries(const char *pathenv)
{
    pathenv_each(pathenv, &print_if_unsafe);
}

void ensure_permitted(void)
{
    if (!in_group(ROOT_GID)) {
        const char *groupname = get_group_name(ROOT_GID);
        if (groupname != NULL) {
            error("You must be in the %s group to run root", groupname);
        }
        else {
            error("You must be in group %u to run root", (unsigned)ROOT_GID);
        }
        exit(ROOT_PERMISSION_DENIED);
    }
}

void become_root(void)
{
    /*
     * XXX
     * should setup_groups be part of become_user() ?
     */
    setup_groups(ROOT_UID);

    if (!become_user(ROOT_UID)) {
        error("Cannot become root");
        /* system error because if this program is installed setuid,
         * then become_user should always succeed */
        exit(ROOT_SYSTEM_ERROR);
    }
}

void run_command(const char *absolute_command, const char *const *args)
{
    /*
     * IMPORTANT
     * This must stay as execv, never execvp.
     *
     * The cast is required because execve doesn't enforce const'ness
     * for backwards compatibility.
     *
     * See
     * http://pubs.opengroup.org/onlinepubs/009695399/functions/exec.html
     * http://stackoverflow.com/questions/190184/execv-and-const-ness
     */
    if (execv(absolute_command, (char *const *)args) == -1) {
        error("Cannot exec '%s': %s", absolute_command, strerror(errno));
        exit(ROOT_ERROR_EXECUTING_COMMAND);
    }
    /* execv does not return on success */
}

void usage(void)
{
    print("Usage: root <command> [<argument>]...");
}

/* vim: set ts=4 sw=4 tw=0 et:*/
