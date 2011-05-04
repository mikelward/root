/*
 * root
 *
 * simple reimplementation of sudo
 *
 * Mikel Ward <mikel@mikelward.com>
 */

/*
 * terminology:
 *  - absolute path     path to a file starting with /
 *  - qualified path    path to a file containing a /
 *  - unqualified path  path to a file not containing a /
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

void usage(void);

void usage(void)
{
    fprintf(stderr, "Usage: root <command> [<argument>]...\n");
}

int main(int argc, char **argv)
{
    if (argc < 2) {
        usage();
        exit(ROOT_INVALID_USAGE);
    }

    initlog(PROGNAME);

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

    /* skip over our own program name */
    argv++, argc--;

    /*
     * The first argument (argv[0]) is the command to run.
     * We will do some checks and processing on it.
     * It will be the first argument to execv(), which tells
     * the system which file to execute.
     *
     * All arguments, including the first, (argv) will be
     * passed verbatim as the second argument to execv().
     */
    const char *command = argv[0];
    char *const *newargv = argv;
    char *absolute_command = NULL;

    if (command == NULL || *command == '\0') {
        error("Command is NULL\n");
        exit(ROOT_INVALID_USAGE);
    }

    debug("Command to run is %s\n", command);

    if (is_qualified_path(command)) {
        /*
         * Command contained a slash, e.g.
         *  - root /path/to/command
         *  - root ./command
         *
         * skip the PATH tests
         */

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
    else {
        /*
         * Command did not contain a slash.
         * Search for it in PATH...
         */
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

        if (!is_absolute_path(qualified_command)) {
            /*
             * We found a relative command via PATH.
             * This means ".", "", or a relative directory was listed in PATH
             * and that directory held the first match for "command".
             *
             * This could result in running a command other than the
             * user expected, which is potentially unsafe, so don't allow it.
             */
            error("You tried to run %s, but this would run %s", command, absolute_command);
            error("Running commands via \"\" or \".\" in PATH is prohibited for security reasons");
            error("Run man 1 root for the reasons and solutions");
            exit(ROOT_RELATIVE_PATH_DISALLOWED);
        }

        free(qualified_command);
    }

    /*
     * We have decided what to run,
     * log the fact before we actually do anything
     *
     * Currently we do this before calling setuid(),
     * because the log message includes the username,
     * and we want to log the calling username, not root
     *
     * XXX log the command arguments too?
     * XXX how to escape control characters,
     *     e.g. what if command name contains backspaces?
     */
    info("Running %s", absolute_command);

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

    /*
     * IMPORTANT
     * This must stay as execv, never execvp.
     */
    if (execv(absolute_command, newargv) == -1) {
        error("Cannot exec '%s': %s", absolute_command, strerror(errno));
        exit(ROOT_ERROR_EXECUTING_COMMAND);
    }
    /* execv does not return on success */
}

/* vim: set ts=4 sw=4 tw=0 et:*/
