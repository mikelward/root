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
#include <limits.h>
#include <pwd.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>

#define _BSD_SOURCE 1

#define PROGNAME "root"
#define PATHENVSEP ":"
#define DIRSEP '/'

/*
 * exit statuses
 *
 * these are set to mimic shell conventions
 */
#define ROOT_PERMISSION_DENIED          1
#define ROOT_INVALID_USAGE              2
#define ROOT_ENVIRONMENT_ERROR          3
#define ROOT_PASSWD_CALL_ERROR          4
#define ROOT_GROUP_CALL_ERROR           5
#define ROOT_RELATIVE_PATH_DISALLOWED   125
#define ROOT_ERROR_EXECUTING_COMMAND    126
#define ROOT_COMMAND_NOT_FOUND          127

/*
 * definitions that administrators may wish to change
 */
const int ROOT_GID = 0;
const int ROOT_UID = 0;
const int verbose = 0;

/*
 * function prototypes
 */

/*
 * print messages when various types of events happen.
 * call it like printf(), do not use a trailing newline.
 */
void debug(const char *format, ...);
void error(const char *format, ...);
void info(const char *format, ...);

/*
 * helpers for above
 */
void writelog(int priority, const char *format, va_list ap);
void writescreen(int priority, const char *format, va_list ap);
char *escape_percents(const char *string);
char *get_username(uid_t uid);

/*
 * helper for main program logic
 */
char *get_command_path(const char *command, const char *pathenv);
int in_group(int root_gid);
int is_unqualified_path(const char *path);
int setup_groups(uid_t uid);

void usage(void);

void writelog(int priority, const char *format, va_list ap)
{
	char *logformat = NULL; size_t logformatmax, logformatlen;
	char *username = NULL; uid_t ruid;

	ruid = getuid();
	username = escape_percents(get_username(ruid));

	logformatmax = strlen(username) + strlen(": ") + strlen(format) + 1;
	logformat = malloc(logformatmax * sizeof *logformat);
	if (logformat == NULL) {
		return;
	}

	/* note no newline: rsyslog doesn't like newlines, vanilla syslog untested */
	logformatlen = snprintf(logformat, logformatmax, "%s: %s", username, format);
	/* XXX what if logformatlen > logformatmax? */

	if (logformatlen > 0) {
		vsyslog(priority, logformat, ap);
	}
	else {
		syslog(LOG_ERR, "Cannot format log message, using default format");
		vsyslog(priority, format, ap);
	}

	free(username);
}

void writescreen(int priority, const char *format, va_list ap)
{
	char *printformat = NULL; size_t printformatmax, printformatlen;
	char *progname = NULL;

	/* only print info and debug messages if verbose flag is set */
	if (priority >= LOG_INFO && !verbose) {
		return;
	}

	progname = escape_percents(PROGNAME);

	printformatmax = strlen(progname) + strlen(": ") + strlen(format) + strlen("\n") + 1;
	printformat = malloc(printformatmax * sizeof *printformat);
	if (printformat == NULL) {
		return;
	}

	printformatlen = snprintf(printformat, printformatmax, "%s: %s\n", progname, format);
	/* XXX what if printformatlen > printformatmax? */

	if (printformatlen > 0) {
		vfprintf(stderr, printformat, ap);
	}
	else {
		syslog(LOG_ERR, "Cannot format screen message, using default format");
		vfprintf(stderr, format, ap);
	}
}

void debug(const char *format, ...)
{
	va_list ap;
	va_start(ap, format);
	writelog(LOG_DEBUG, format, ap);
	writescreen(LOG_DEBUG, format, ap);
	va_end(ap);
}

void error(const char *format, ...)
{
	va_list ap;
	va_start(ap, format);
	writelog(LOG_ERR, format, ap);
	writescreen(LOG_ERR, format, ap);
	va_end(ap);
}

void info(const char *format, ...)
{
	va_list ap;
	va_start(ap, format);
	writelog(LOG_INFO, format, ap);
	writescreen(LOG_INFO, format, ap);
	va_end(ap);
}

/* caller must free returned string */
char *escape_percents(const char *string)
{
	char *escaped;
	size_t length;
	size_t spos, epos;
	
	length = strlen(string);
	escaped = malloc(length * 2 + 1);

	if (escaped == NULL) {
		return NULL;
	}

	for (spos = 0, epos = 0; string[spos] != '\0'; spos++) {
		if (string[spos] == '%') {
			escaped[epos++] = '%';
			escaped[epos++] = '%';
		}
		else {
			escaped[epos++] = string[spos];
		}
	}
	escaped[epos] = '\0';

	return escaped;
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
	size_t commandlen = strlen(command);
	char *pathenvcopy = strdup(pathenv);
	char *match = NULL;
	for (char *dir = strtok(pathenvcopy, PATHENVSEP);
	     dir != NULL;
	     dir = strtok(NULL, PATHENVSEP)) {

		int dirlen = strlen(dir);
		char *path = malloc(dirlen+1+commandlen+1);
		if (path == NULL) {
			error("Cannot allocate memory to hold path");
			return NULL;
		}
		strcpy(path, dir);

		debug("Looking in %s", path);

		if (strcmp(path, "") != 0 && path[dirlen-1] != DIRSEP) {
			char dirsepstr[2];
			sprintf(dirsepstr, "%c", DIRSEP);
			strcat(path, dirsepstr);
			dirlen++;
		}
		strcat(path, command);

		if (access(path, F_OK) == 0) {
			debug("%s is %s", command, path);

			return path;
		}
		else {
			free(path);
			path = NULL;
		}
	}
	debug("%s not found in PATH", command);
	return NULL;
}

char *get_username(uid_t uid)
{
	struct passwd *ppw = getpwuid(uid);
	if (ppw == NULL) {
		return "Unknown user";
	}

	return ppw->pw_name;
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
			error("Cannot determine maximum number of groups: %s", strerror(errno));
			exit(ROOT_GROUP_CALL_ERROR);
		}
		else {
			int ngroups;
			gid_t grouplist[ngroups_max];

			errno = 0;
			ngroups = getgroups(ngroups_max, grouplist);
			if (ngroups == -1) {
				error("Cannot get group list: %s", strerror(errno));
				exit(ROOT_GROUP_CALL_ERROR);
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
		error("Cannot get passwd info for uid %d: %s", uid, strerror(errno));
		exit(ROOT_PASSWD_CALL_ERROR);
	}
	else {
		int result;

		errno = 0;
		result = setgid(ps->pw_gid);
		if (result == -1) {
			error("Cannot setgid %d: %s", ps->pw_gid, strerror(errno));
			exit(ROOT_PERMISSION_DENIED);
		}

		errno = 0;
		result = initgroups(ps->pw_name, ps->pw_gid);
		if (result == -1) {
			error("Cannot initgroups for %s: %s", ps->pw_name, strerror(errno));
			exit(ROOT_GROUP_CALL_ERROR);
		}
		else {
			return 0;
		}
	}
}

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

	openlog(PROGNAME, LOG_CONS|LOG_PID, LOG_AUTHPRIV);

	is_allowed = in_group(ROOT_GID);
	if (is_allowed) {
		int status;
		const char *command = argv[1];
		char *const *newargv = argv+1;
		char *command_path = NULL;

		if (is_unqualified_path(command)) {
			debug("Unqualified path, will search PATH");
			const char *pathenv = getenv("PATH");
			if (pathenv == NULL) {
				error("Cannot get PATH environment variable");
				exit(ROOT_ENVIRONMENT_ERROR);
			}
			/*debug("PATH=%s", pathenv);*/
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
			debug("Qualified path, bypassing PATH");
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
		struct group *gp = getgrgid(ROOT_GID);
		if (gp != NULL && gp->gr_name != NULL) {
			error("You must be in the %s group to run root", gp->gr_name);
		}
		else {
			error("You must be in group 0 to run root");
		}
		exit(ROOT_PERMISSION_DENIED);
	}
}

/* vim: set sw=4 ts=4 noet: */
