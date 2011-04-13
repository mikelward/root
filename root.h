#ifndef ROOT_H
#define ROOT_H

#define PROGNAME "root"

#define ROOT_GID 0
#define ROOT_UID 0

/*
 * exit statuses
 *
 * these are set to mimic shell conventions
 *
 * XXX re-evaluate whether these are appropriate
 *     maybe it should just be one exit code
 *     so it doesn't mask the called program's exit status
 */
#define ROOT_PERMISSION_DENIED          1
#define ROOT_INVALID_USAGE              2
#define ROOT_ENVIRONMENT_ERROR          3
#define ROOT_PASSWD_CALL_ERROR          4
#define ROOT_GROUP_CALL_ERROR           5
#define ROOT_RELATIVE_PATH_DISALLOWED   125
#define ROOT_ERROR_EXECUTING_COMMAND    126
#define ROOT_COMMAND_NOT_FOUND          127

#endif
/* vim: set ts=4 sw=4 tw=0 et:*/
