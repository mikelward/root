/*
 * root
 *
 * simple reimplementation of sudo
 *
 * $Id$
 */

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
	int is_allowed;

	if (argc < 2) {
		usage();
		exit(2);
	}

	initlog(PROGNAME);

	is_allowed = in_group(ROOT_GID);
	if (is_allowed) {
		int status;
		const char *command = argv[1];
		char *const *newargv = argv+1;
		char *command_path = NULL;

		debug("Command to run is %s\n", command);

		if (is_unqualified_path(command)) {
			/*debug("Unqualified path, will search PATH");*/
			const char *pathenv = getenv("PATH");
			if (pathenv == NULL) {
				error("Cannot get PATH environment variable");
				exit(ROOT_ENVIRONMENT_ERROR);
			}
			debug("PATH=%s", pathenv);
			command_path = get_command_path(command, pathenv);
			if (command_path == NULL) {
				error("Cannot find %s in PATH", command);
				exit(ROOT_COMMAND_NOT_FOUND);
			} else if (command_path[0] != DIRSEP) {
				/*
				* don't allow running a command in the current directory
				* or any command with a relative path
				* (unless user specified a qualified path, in which case
				*  we don't enter this function anyway)
				*/
				char *resolved_path = realpath(command_path, NULL);
				if (resolved_path != NULL)
					error("Not running %s: Resolves to relative command %s", command, resolved_path);
				else
					error("Not running %s: Found via current or relative directory in PATH", command);
				exit(ROOT_RELATIVE_PATH_DISALLOWED);
			}
		}
		else {
			/*debug("Qualified path, bypassing PATH");*/
			command_path = strdup(command);
		}

		/*
		 * We have decided what to run,
		 * log the fact before we actually do anything
		 */
		info("Running %s", command_path);

		errno = 0;
		status = setuid(ROOT_UID);
		/*status = setreuid(ROOT_UID, ROOT_UID);*/
		if (status == -1) {
			error("Cannot setuid %d: %s", ROOT_UID, strerror(errno));
			exit(ROOT_PERMISSION_DENIED);
		}

		setup_groups(ROOT_UID);

		errno = 0;

		/*
		 * IMPORTANT
		 * This must stay as execv, never execvp.
		 */
		status = execv(command_path, newargv);
		if (status == -1) {
			error("Cannot exec '%s': %s", command_path, strerror(errno));
			exit(ROOT_ERROR_EXECUTING_COMMAND);
		}
		else {
			/* execvp does not return on success */
		}
	}
	else {
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

/* vim: set sw=4 ts=4 noet: */
