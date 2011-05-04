#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "root.h"
#include "path.h"
#include "logging.h"

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
    size_t commandlen = strlen(command);
    char *pathenvcopy = strdup(pathenv);
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

        /*debug("Looking in %s", path);*/

        if (strcmp(path, "") != 0 && path[dirlen-1] != DIRSEP) {
            char dirsepstr[2];
            sprintf(dirsepstr, "%c", DIRSEP);
            strcat(path, dirsepstr);
            dirlen++;
        }
        strcat(path, command);

        if (access(path, F_OK) == 0) {
            /*debug("%s is %s", command, path);*/

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

/**
 * Returns 0 (true) if path does not contain a slash.
 *
 * This is typically used to determine whether a command
 * should be looked up in PATH or executed as-is.
 */
int is_unqualified_path(const char *path)
{
    return !is_qualified_path(path);
}

/**
 * Returns 0 (true) if path contains a slash.
 *
 * This is typically used to determine whether a command
 * should be looked up in PATH or executed as-is.
 */
int is_qualified_path(const char *path)
{
    return path && strchr(path, DIRSEP) != NULL;
}

/**
 * Returns 0 (true) if path is an unambigious path starting at the root.
 */
int is_absolute_path(const char *path)
{
    return path && path[0] == DIRSEP;
}

/* vim: set ts=4 sw=4 tw=0 et:*/
