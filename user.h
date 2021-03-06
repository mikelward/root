#ifndef USER_H
#define USER_H

#include <pwd.h> /* for uid_t and gid_t */

char *get_group_name(gid_t gid);
int in_group(gid_t root_gid);
int setup_groups(uid_t uid);
int set_home_dir(uid_t uid);
int become_user(uid_t uid);

#endif
/* vim: set ts=4 sw=4 tw=0 et:*/
