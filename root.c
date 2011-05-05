/*
 * root
 *
 * simple reimplementation of sudo
 *
 * Mikel Ward <mikel@mikelward.com>
 */

#include "root.h"

static void setup_logging();
static void process_args(int argc, const char * const *argv,
                         char **absolute_commandp, const char * const **argsp);
static void ensure_permitted(void);
static void become_root(void);
static void run_command(const char *absolute_command, const char * const *args);
static void usage(void);

int main(int argc, const char * const *argv)
{
    char *absolute_command = NULL;
    const char * const *args = NULL;

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
 * This is the most complicated part, but the concept is simple.
 *
 * We read the command line arguments to figure out what command to run.
 *
 * The first argument (argv[0] -> *absolute_commandp) is the command.
 *
 * Any command of the form "/path/to/command" is allowed.
 * Any command of the form "./command" is allowed.
 * Any command of the form "command", is only allowed if it is safe.
 *
 * A safe command is one that forms an absolute path after being
 * looked up in PATH, e.g. "ls" if the first match in PATH is "/bin".
 * An unsafe command is one that forms a relative path after being
 * looked up in PATH, e.g. "ls" if the first match in PATH is "./ls"
 * (where . is the current directory).
 *
 * We could just check for a command starting with a dot, but I also
 * want to handle the case where somebody puts something strange like
 * "bin" in PATH (as opposed to "/bin"), which could also result in
 * running an unintended command.  Ensuring the resultant path is
 * absolute handles both cases.
 *
 * The remaining arguments (argv -> *argsp)
 * (including argv[0] due to C/UNIX conventions)
 * are the arguments that will be passed unchanged to execv().
 *
 * Terminology:
 *  - absolute path     path starting with /     (e.g. /path/to/command)
 *  - qualified path    path containing a /      (e.g. ./command)
 *  - unqualified path  path not containing a /  (e.g. command)
 */
void process_args(int argc, const char * const *argv,
                  char **absolute_commandp, const char * const **argsp)
{
    if (argc < 2) {
        usage();
        exit(ROOT_INVALID_USAGE);
    }

    if (absolute_commandp == NULL) {
        error("process_args: absolute_commandp is NULL\n");
        exit(ROOT_PROGRAMMER_ERROR);
    }

    if (argsp == NULL) {
        error("process_args: argsp is NULL\n");
        exit(ROOT_PROGRAMMER_ERROR);
    }

    /* skip over our own program name */
    argv++, argc--;

    char *absolute_command = NULL;
    const char *command = argv[0];
    if (command == NULL || *command == '\0') {
        error("Command is NULL\n");
        exit(ROOT_INVALID_USAGE);
    }

    debug("Command to run is %s\n", command);

    /*
     * If the command contains a slash, it is always allowed.
     */
    if (is_qualified_path(command)) {

        /*
         * Determine the canonical, absolute path to command
         * for passing to execv().
         */
        absolute_command = realpath(command, NULL);
        if (absolute_command == NULL) {
            error("Cannot determine real path to %s\n", command);
            exit(ROOT_COMMAND_NOT_FOUND);
        }
    }
    /*
     * If the command does not contain a slash, search for it in PATH.
     */
    else {
        const char *pathenv = getenv("PATH");
        if (pathenv == NULL) {
            error("Cannot get PATH environment variable");
            exit(ROOT_SYSTEM_ERROR);
        }
        debug("Searching for command in PATH=%s", pathenv);
        char *qualified_command = get_command_path(command, pathenv);
        if (qualified_command == NULL) {
            error("Cannot find %s in PATH", command);
            exit(ROOT_COMMAND_NOT_FOUND);
        }

        /*
         * Determine the canonical, absolute path to command
         * for passing to execv().
         *
         * Logically, this belongs at the bottom of this else branch,
         * but we do it early so that if we prohibit running the command,
         * we can tell the user what command really would have been run
         * (e.g. so we can print "/tmp/ls" rather than "./ls").
         */
        absolute_command = realpath(qualified_command, NULL);
        if (absolute_command == NULL) {
            error("Cannot determine real path to %s\n", qualified_command);
            exit(ROOT_COMMAND_NOT_FOUND);
        }

        /*
         * Ensure the path is an absolute PATH to prevent running
         * a malicious program installed by a rogue user in the current
         * directory (or any relative entry in PATH).
         */
        if (!is_absolute_path(qualified_command)) {
            error("You tried to run %s, but this would run %s", command, absolute_command);
            error("Running commands via \"\" or \".\" in PATH is prohibited for security reasons");
            error("Run man 1 root for the reasons and solutions");
            exit(ROOT_RELATIVE_PATH_DISALLOWED);
        }

        free(qualified_command);
    }

    *absolute_commandp = absolute_command;
    *argsp = argv;
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

void run_command(const char *absolute_command, const char * const *args)
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
    if (execv(absolute_command, (char * const *)args) == -1) {
        error("Cannot exec '%s': %s", absolute_command, strerror(errno));
        exit(ROOT_ERROR_EXECUTING_COMMAND);
    }
    /* execv does not return on success */
}

void usage(void)
{
    fprintf(stderr, "Usage: root <command> [<argument>]...\n");
}

/* vim: set ts=4 sw=4 tw=0 et:*/
