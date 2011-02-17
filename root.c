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

const int ROOT_GID=0;
const int ROOT_UID=0;

void error(const char *format, ...)
{
	va_list ap;
	va_start(ap, format);
	vfprintf(stderr, format, ap);
	va_end(ap);
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
			int i, ngroups;
			gid_t grouplist[ngroups_max];

			errno = 0;
			ngroups = getgroups(ngroups_max, grouplist);
			if (ngroups == -1) {
				error("Cannot get group list: %s\n", strerror(errno));
				exit(1);
			}

			for (i = 0; i < ngroups; i++) {
				if (grouplist[i] == root_gid) {
					return 1;
				}
			}
			return 0;
		}
	}
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
	error("Usage: <program> <program parameter>...\n");
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
		const char *file = argv[1];
		char *const *newargv = argv+1;

		errno = 0;
		status = setuid(ROOT_UID);
		/*status = setreuid(ROOT_UID, ROOT_UID);*/
		if (status == -1) {
			error("Cannot setuid %d: %s\n", ROOT_UID, strerror(errno));
			exit(1);
		}

		setup_groups(ROOT_UID);

		errno = 0;
		status = execvp(file, newargv);
		if (status == -1) {
			error("Cannot exec %s: %s\n", file, strerror(errno));
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
