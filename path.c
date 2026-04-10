#define _DEFAULT_SOURCE /* for strdup(), glibc >= 2.20 */
#define _BSD_SOURCE     /* for strdup() */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "root.h"
#include "path.h"
#include "logging.h"

/*
 * Return the full path to command found by searching for it in pathenv,
 * and returning the first match.
 *
 * If command is found via a relative path in PATH (e.g. "" or "."),
 * the relative path is still returned. The caller is responsible for
 * checking whether the result is safe (e.g. via is_absolute_path()).
 *
 * If the string returned is not NULL, it must be freed by the caller.
 */
char *get_command_path(const char *command, const char *pathenv)
{
    if (command == NULL || *command == '\0') {
        error("get_command_path: command is empty");
        return NULL;
    }
    if (pathenv == NULL) {
        error("get_command_path: pathenv is NULL");
        return NULL;
    }

    size_t commandlen = strlen(command);
    char *pathenvcopy = strdup(pathenv);
    if (pathenvcopy == NULL) {
        error("Cannot allocate memory to hold pathenv");
        return NULL;
    }

    char *remaining = pathenvcopy;
    while (remaining != NULL) {
        char *sep = strchr(remaining, PATHENVSEP[0]);
        char *dir;
        if (sep != NULL) {
            *sep = '\0';
            dir = remaining;
            remaining = sep + 1;
        }
        else {
            dir = remaining;
            remaining = NULL;
        }

        /* POSIX: an empty PATH entry means the current directory */
        if (*dir == '\0') {
            dir = ".";
        }

        size_t dirlen = strlen(dir);
        char *path = malloc(dirlen + 1 + commandlen + 1);
        if (path == NULL) {
            error("Cannot allocate memory to hold path");
            free(pathenvcopy);
            return NULL;
        }
        strcpy(path, dir);

        /*debug("Looking in %s", path);*/

        if (path[dirlen - 1] != DIRSEP) {
            char dirsepstr[2];
            sprintf(dirsepstr, "%c", DIRSEP);
            strcat(path, dirsepstr);
            dirlen++;
        }
        strcat(path, command);

        if (access(path, X_OK) == 0) {
            struct stat st;
            if (stat(path, &st) == -1) {
                error("Cannot stat %s: %s", path, strerror(errno));
                free(path);
                continue;
            }
            if (S_ISDIR(st.st_mode)) {
                free(path);
                continue;
            }

            /*debug("%s is %s", command, path);*/

            free(pathenvcopy);
            return path;
        }
        else {
            free(path);
            path = NULL;
        }
    }

    debug("%s not found in PATH", command);
    free(pathenvcopy);
    return NULL;
}

/**
 * Returns non-zero (true) if path does not contain a slash.
 *
 * This is typically used to determine whether a command
 * should be looked up in PATH or executed as-is.
 */
int is_unqualified_path(const char *path)
{
    return path != NULL && strchr(path, DIRSEP) == NULL;
}

/**
 * Returns non-zero (true) if path contains a slash.
 *
 * This is typically used to determine whether a command
 * should be looked up in PATH or executed as-is.
 */
int is_qualified_path(const char *path)
{
    return path && strchr(path, DIRSEP) != NULL;
}

/**
 * Returns non-zero (true) if path is an unambiguous path starting at the root.
 */
int is_absolute_path(const char *path)
{
    return path && path[0] == DIRSEP;
}

/**
 * Perform an action on each element of PATH.
 */
void pathenv_each(const char *pathenv, void (*func)(const char *pathentry))
{
    if (pathenv == NULL) {
        error("pathenv_each: pathenv is NULL");
        return;
    }

    char *pathenvcopy = strdup(pathenv);
    if (pathenvcopy == NULL) {
        error("Cannot allocate memory to hold pathenv");
        return;
    }

    char *remaining = pathenvcopy;
    while (remaining != NULL) {
        char *sep = strchr(remaining, PATHENVSEP[0]);
        char *dir;
        if (sep != NULL) {
            *sep = '\0';
            dir = remaining;
            remaining = sep + 1;
        }
        else {
            dir = remaining;
            remaining = NULL;
        }

        /* POSIX: an empty PATH entry means the current directory */
        if (*dir == '\0') {
            dir = ".";
        }

        (*func)(dir);
    }

    free(pathenvcopy);
}


/* vim: set ts=4 sw=4 tw=0 et:*/
