#define _BSD_SOURCE				/* for initgroups() */

#include <sys/types.h>
#include <errno.h>
#include <grp.h>
#include <pwd.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "root.h"
#include "user.h"
#include "logging.h"

char *get_group_name(gid_t gid)
{
    struct group *gp = getgrgid(gid);
    if (gp != NULL && gp->gr_name != NULL) {
        return gp->gr_name;
    }
    else {
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

/* vim: set ts=4 sw=4 tw=0 et:*/
