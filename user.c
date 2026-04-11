#define _DEFAULT_SOURCE /* for initgroups(), glibc >= 2.20 */
#define _BSD_SOURCE     /* for initgroups() */

#include <sys/types.h>
#include <errno.h>
#include <grp.h>
#include <pwd.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "logging.h"
#include "root.h"
#include "user.h"

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

int in_group(gid_t root_gid)
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
            error("Cannot determine maximum number of groups: %s",
                  strerror(errno));
            exit(ROOT_SYSTEM_ERROR);
        }
        else {
            int ngroups;
            gid_t *grouplist = malloc(ngroups_max * sizeof(gid_t));
            if (grouplist == NULL) {
                error("Cannot allocate memory for group list");
                exit(ROOT_SYSTEM_ERROR);
            }

            errno = 0;
            ngroups = getgroups(ngroups_max, grouplist);
            if (ngroups == -1) {
                error("Cannot get group list: %s", strerror(errno));
                free(grouplist);
                exit(ROOT_SYSTEM_ERROR);
            }

            for (int i = 0; i < ngroups; i++) {
                if (grouplist[i] == root_gid) {
                    free(grouplist);
                    return 1;
                }
            }
            free(grouplist);
            return 0;
        }
    }
}

/*
 * set up groups for the target uid
 *
 * calls exit() on failure because these are unrecoverable system errors
 * and we don't want to leave the process in a partially modified state
 * (e.g. setgid succeeded but initgroups failed)
 */
int setup_groups(uid_t uid)
{
    struct passwd *ps;
    int result;

    errno = 0;
    ps = getpwuid(uid);
    if (ps == NULL) {
        error("Cannot get passwd info for uid %lu: %s", (unsigned long)uid, strerror(errno));
        exit(ROOT_SYSTEM_ERROR);
    }

    errno = 0;
    result = setgid(ps->pw_gid);
    if (result == -1) {
        error("Cannot setgid %lu: %s", (unsigned long)ps->pw_gid, strerror(errno));
        exit(ROOT_SYSTEM_ERROR);
    }

    errno = 0;
    result = initgroups(ps->pw_name, ps->pw_gid);
    if (result == -1) {
        error("Cannot initgroups for %s: %s", ps->pw_name, strerror(errno));
        exit(ROOT_SYSTEM_ERROR);
    }
    else {
        return 1;
    }
}

/*
 * set the $HOME environment variable to the target uid's home directory
 *
 * returns 1 (true) on success, 0 (false) on setenv failure
 * calls exit() if getpwuid fails (unrecoverable system error)
 */
int set_home_dir(uid_t uid)
{
    struct passwd *ps;

    errno = 0;
    ps = getpwuid(uid);
    if (ps == NULL) {
        error("Cannot get passwd info for uid %lu: %s", (unsigned long)uid, strerror(errno));
        exit(ROOT_SYSTEM_ERROR);
    }

    return setenv("HOME", ps->pw_dir, 1) == 0;
}

/*
 * become the specified user
 *
 * currently the only supported user is root (uid=0)
 *
 * returns 1 (true) on success, 0 (false) if uid != 0
 * calls exit() if setuid fails (unrecoverable system error)
 */
int become_user(uid_t uid)
{
    if (uid != 0) {
        error("Becoming non-root user has not been tested");
        return 0;
    }

    /*
     * root should be installed setuid root
     *
     * before setuid:
     * ruid = user, euid = root, suid = root
     * after setuid(0):
     * ruid = root, euid = root, suid = root
     */
    errno = 0;
    if (setuid(uid) == -1) {
        error("Cannot setuid %lu: %s", (unsigned long)uid, strerror(errno));
        exit(ROOT_SYSTEM_ERROR);
    }
    return 1;
}

/* vim: set ts=4 sw=4 tw=0 et:*/
