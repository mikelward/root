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
     * first argument is the command to run,
     * which we will do some checks on first;
     * all arguments (including the first)
     * will be passed verbatim to exec as argv
     */
    const char *command = argv[0];
    char *const *newargv = argv;
    char *command_path = NULL;

    debug("Command to run is %s\n", command);

    /*
     * Allowed:
     * - root /path/to/command
     * - root ./command
     *
     * Allowed only if the first matching entry in PATH starts with a "/":
     * - root command
     */
    if (is_unqualified_path(command)) {
        /*debug("Unqualified path, will search PATH");*/
        const char *pathenv = getenv("PATH");
        if (pathenv == NULL) {
            error("Cannot get PATH environment variable");
            exit(ROOT_SYSTEM_ERROR);
        }
        debug("PATH=%s", pathenv);
        command_path = get_command_path(command, pathenv);
        if (command_path == NULL) {
            error("Cannot find %s in PATH", command);
            exit(ROOT_COMMAND_NOT_FOUND);
        }
        else if (command_path[0] != DIRSEP) {
            /*
             * don't allow running a command in the current directory
             * or any command with a relative path
             * (unless user specified a qualified path, in which case
             *  we don't enter this function anyway)
             */
            char *resolved_path = realpath(command_path, NULL);
            if (resolved_path != NULL) {
                error("You tried to run %s, but this would run %s", command, resolved_path);
            }
            else {
                error("You tried to run %s, which is a relative path", command, resolved_path);
            }
            error("Running commands via \"\" or \".\" in PATH is prohibited for security reasons");
            error("Run man 1 root for the reasons and solutions");
            exit(ROOT_RELATIVE_PATH_DISALLOWED);
        }
    }
    else {
        /*debug("Qualified path, bypassing PATH");*/
        command_path = strdup(command);
    }

    /*
     * TODO
     * consider converting command_path into an absolute path
     * so that any log message mentioning the command is unambiguous
     */

    /*
     * We have decided what to run,
     * log the fact before we actually do anything
     *
     * Currently we do this before calling setuid(),
     * because the log message includes the username,
     * and we want to log the calling username, not root
     */
    info("Running %s", command_path);

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
    if (execv(command_path, newargv) == -1) {
        error("Cannot exec '%s': %s", command_path, strerror(errno));
        exit(ROOT_ERROR_EXECUTING_COMMAND);
    }
    /* execv does not return on success */
}

/* vim: set ts=4 sw=4 tw=0 et:*/
