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
#define ROOT_INVALID_USAGE              122
#define ROOT_PERMISSION_DENIED          123
#define ROOT_SYSTEM_ERROR               124
#define ROOT_RELATIVE_PATH_DISALLOWED   125
#define ROOT_ERROR_EXECUTING_COMMAND    126
#define ROOT_COMMAND_NOT_FOUND          127

#endif
/* vim: set ts=4 sw=4 tw=0 et:*/
