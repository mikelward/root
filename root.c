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
 * caller must free returned path
 */
char *get_unique_path(const char *command, const char *pathenv)
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
		strcpy(path, dir);

		if (strcmp(dir, "") == 0 ||
			strcmp(dir, ".") == 0) {
			/* XXX test this only if path exists */
			if (matches == 0) {
				error("Ignoring current directory in PATH\n");
				return NULL;
			}
			else {
				debug("Skipping %s\n", path);
				continue;
			}
		}

		debug("Looking in %s\n", path);

		/* safe because we handled len==0 above */
		if (path[dirlen-1] != DIRSEP) {
			char dirsepstr[2];
			sprintf(dirsepstr, "%c", DIRSEP);
			strcat(path, dirsepstr);
			dirlen++;
		}
		/*
		if (commandlen > PATH_MAX - dirlen) {
			error("Path is too long: %s%s\n", path, command);
			exit(1);
		}
		*/
		strcat(path, command);

		if (access(path, F_OK) == 0) {
			match = path;
			matches++;
			debug("%s is %s\n", command, path);
		}
		else {
			free(path);
			path = NULL;
		}
	}
	if (matches == 1) {
		return match;
	}
	else if (matches == 0) {
		error("%s not found\n", command);
		return NULL;
	}
	else {
		error("%s appears in PATH multiple times\n", command);
		return NULL;
	}
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

int is_relative_path(const char *path)
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
		char *unique_command = NULL;

		if (is_relative_path(command)) {
			const char *pathenv = getenv("PATH");
			if (pathenv == NULL) {
				error("Cannot get PATH environment variable\n");
				exit(1);
			}
			unique_command = get_unique_path(command, pathenv);
			if (unique_command == NULL) {
				error("Cannot determine path to %s\n", command);
				exit(1);
			}
		}
		else {
			unique_command = strdup(command);
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

		/* TODO confirm newargv is correct */
		status = execv(unique_command, newargv);
		if (status == -1) {
			error("Cannot exec '%s': %s\n", unique_command, strerror(errno));
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
