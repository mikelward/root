/*
 * root
 *
 * simple reimplementation of sudo
 *
 * $Id$
 */

#include <sys/types.h>
#include <errno.h>
#include <grp.h>
#include <pwd.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define _BSD_SOURCE 1

#define PROGNAME root
#define PATHENVSEP ":"
#define DIRSEP '/'

const int ROOT_GID = 0;
const int ROOT_UID = 0;
const int verbose = 1;

void debug(const char *format, ...)
{
	va_list ap;
	va_start(ap, format);
	if (verbose)
		vfprintf(stderr, format, ap);
	va_end(ap);
}

void error(const char *format, ...)
{
	va_list ap;
	va_start(ap, format);
	vfprintf(stderr, format, ap);
	va_end(ap);
}

/*
 * Return the full path to command found by searching for it in pathenv,
 * and returning the first match.
 *
 * If command is found via a non-absolute path in PATH, return NULL.
 * This is to prevent security issues arising from "" or "." in PATH.
 *
 * If the string returned is not NULL, it must be freed by the caller.
 */
char *get_command_path(const char *command, const char *pathenv)
{
	int matches = 0;
	int commandlen = strlen(command);
	char *pathenvcopy = strdup(pathenv);
	char *match = NULL;
	for (char *dir = strtok(pathenvcopy, PATHENVSEP);
	     dir != NULL;
	     dir = strtok(NULL, PATHENVSEP)) {

		int dirlen = strlen(dir);
		char *path = malloc(dirlen+1+commandlen+1);
		if (path == NULL) {
			error("Cannot allocate memory to hold path\n");
			return NULL;
		}
		strcpy(path, dir);

		debug("Looking in %s\n", path);

		if (strcmp(path, "") != 0 && path[dirlen-1] != DIRSEP) {
			char dirsepstr[2];
			sprintf(dirsepstr, "%c", DIRSEP);
			strcat(path, dirsepstr);
			dirlen++;
		}
		strcat(path, command);

		if (access(path, F_OK) == 0) {
			debug("%s is %s\n", command, path);

			/*
			 * don't allow running a command in the current directory
			 * or any command with a relative path
			 * (unless user specified a qualified path, in which case
			 *  we don't enter this function anyway)
			 */
			if (path[0] != DIRSEP) {
				error("Not running %s: found via current or relative directory in PATH\n", path);
				return NULL;
			}

			return path;
		}
		else {
			free(path);
			path = NULL;
		}
	}
	error("%s not found in PATH\n", command);
	return NULL;
}

int in_group(int root_gid)
{
	gid_t gid;

	gid = getgid();

	if (gid == root_gid) {
		return 1;
	}
	else {
		long ngroups_max;
		errno = 0;
		ngroups_max = sysconf(_SC_NGROUPS_MAX);
		if (ngroups_max == -1) {
			error("Cannot determine maximum number of groups: %s\n", strerror(errno));
			exit(1);
		}
		else {
			int ngroups;
			gid_t grouplist[ngroups_max];

			errno = 0;
			ngroups = getgroups(ngroups_max, grouplist);
			if (ngroups == -1) {
				error("Cannot get group list: %s\n", strerror(errno));
				exit(1);
			}

			for (int i = 0; i < ngroups; i++) {
				if (grouplist[i] == root_gid) {
					return 1;
				}
			}
			return 0;
		}
	}
}

/**
 * Returns 0 (true) if path does not contain a slash.
 * 
 * This is typically used to determine whether a command
 * should be looked up in PATH or executed as-is.
 */
int is_unqualified_path(const char *path)
{
	return strchr(path, DIRSEP) == NULL;
}

int setup_groups(uid_t uid)
{
	struct passwd *ps;

	errno = 0;
	ps = getpwuid(uid);
	if (ps == NULL) {
		error("Cannot get passwd info for uid %d: %s\n", uid, strerror(errno));
		exit(1);
	}
	else {
		int result;

		errno = 0;
		result = setgid(ps->pw_gid);
		if (result == -1) {
			error("Cannot setgid %d: %s\n", ps->pw_gid, strerror(errno));
			exit(1);
		}

		errno = 0;
		result = initgroups(ps->pw_name, ps->pw_gid);
		if (result == -1) {
			error("Cannot initgroups for %s: %s\n", ps->pw_name, strerror(errno));
			exit(1);
		}
		else {
			return 0;
		}
	}
}

void usage(void)
{
	error("Usage: root <command> [<argument>]...\n");
}

int main(int argc, char **argv)
{
	int is_allowed;

	if (argc < 2) {
		usage();
		exit(2);
	}

	is_allowed = in_group(ROOT_GID);
	if (is_allowed) {
		int status;
		const char *command = argv[1];
		char *const *newargv = argv+1;
		char *command_path = NULL;

		if (is_unqualified_path(command)) {
			debug("Unqualified path to command, will search PATH\n");
			const char *pathenv = getenv("PATH");
			if (pathenv == NULL) {
				error("Cannot get PATH environment variable\n");
				exit(1);
			}
			/*debug("PATH=%s\n", pathenv);*/
			command_path = get_command_path(command, pathenv);
			if (command_path == NULL) {
				/*error("Cannot determine path to %s\n", command);*/
				exit(1);
			}
		}
		else {
			command_path = strdup(command);
		}

		errno = 0;
		status = setuid(ROOT_UID);
		/*status = setreuid(ROOT_UID, ROOT_UID);*/
		if (status == -1) {
			error("Cannot setuid %d: %s\n", ROOT_UID, strerror(errno));
			exit(1);
		}

		setup_groups(ROOT_UID);

		errno = 0;

		status = execv(command_path, newargv);
		if (status == -1) {
			error("Cannot exec '%s': %s\n", command_path, strerror(errno));
			exit(1);
		}
		else {
			/* execvp does not return on success */
		}
	}
	else {
		struct group *gp = getgrgid(ROOT_GID);
		if (gp != NULL && gp->gr_name != NULL) {
			error("You must be in the %s group to run root\n", gp->gr_name);
		}
		else {
			error("You must be in group 0 to run root\n");
		}
		exit(1);
	}
}

/* vim: set sw=4 ts=4 noet: */
