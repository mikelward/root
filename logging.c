#define _BSD_SOURCE             /* for strdup() */

#include <sys/types.h>
#include <errno.h>
#include <pwd.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>

#include "logging.h"

int loglevel = LOG_NOTICE;        /* only print NOTICE, ERROR, CRIT, ... */
static const char *g_progname;    /* XXX? maybe share this with root.o */

void initlog(const char *name)
{
    openlog(name, LOG_CONS|LOG_PID, LOG_AUTHPRIV);
    g_progname = strdup(name);
}

/*
 * Return a string in the format "<tag>: <format><suffix>".
 *
 * Caller must free returned string.
 */
char *makeformat(const char *tag, const char *format, const char *suffix)
{
    char *fmt = NULL; size_t fmtmax, fmtlen;

    fmtmax = strlen(tag) + strlen(": ") + strlen(format) + strlen(suffix) + 1;
    fmt = malloc(fmtmax * sizeof *format);
    if (fmt == NULL) {
        return NULL;
    }

    fmtlen = snprintf(fmt, fmtmax, "%s: %s%s", tag, format, suffix);
    if (fmtlen >= fmtmax) {
        /* Don't call writescreen or writelog, since that's how we got here. */
        fprintf(stderr, "root: Unable to make log format\n");
        syslog(LOG_CRIT, "root: Unable to make log format");
        exit(1);
    }

    return fmt;
}

void writelog(int priority, const char *format, va_list ap)
{
    char *logformat = NULL;
    char *escapedusername = NULL;
    uid_t ruid;

    ruid = getuid();
    escapedusername = escape_percents(get_username(ruid));
    logformat = makeformat(escapedusername, format, "");

    vsyslog(priority, logformat, ap);

    free(escapedusername);
    free(logformat);
}

void writescreen(int priority, const char *format, va_list ap)
{
    char *screenformat = NULL;
    char *escapedprogname = NULL;

    /* only print messages at loglevel or "lower" priority */
    /* with syslog, lowest means most important */
    if (priority > loglevel) {
        return;
    }

    escapedprogname = escape_percents(g_progname);
    screenformat = makeformat(escapedprogname, format, "\n");
    vfprintf(stderr, screenformat, ap);

    free(escapedprogname);
    free(screenformat);
}

void debug(const char *format, ...)
{
    va_list ap;
    va_start(ap, format);
    writelog(LOG_DEBUG, format, ap);
    va_end(ap);
    va_start(ap, format);
    writescreen(LOG_DEBUG, format, ap);
    va_end(ap);
}

void error(const char *format, ...)
{
    va_list ap;
    va_start(ap, format);
    writelog(LOG_ERR, format, ap);
    va_end(ap);
    va_start(ap, format);
    writescreen(LOG_ERR, format, ap);
    va_end(ap);
}

/*
 * XXX how to escape control characters,
 *     e.g. what if command name contains backspaces?
 */
void info(const char *format, ...)
{
    va_list ap;
    va_start(ap, format);
    writelog(LOG_INFO, format, ap);
    va_end(ap);
    va_start(ap, format);
    writescreen(LOG_INFO, format, ap);
    va_end(ap);
}

/*
 * Print the given message on stderr.
 *
 * Arguments are just log printf.
 *
 * Note that print does not add an implicit newline.
 */
void print(const char *format, ...)
{
    va_list ap;
    va_start(ap, format);
    vfprintf(stderr, format, ap);
    va_end(ap);
}

char *get_username(uid_t uid)
{
    struct passwd *ppw = getpwuid(uid);
    if (ppw == NULL) {
        return "Unknown user";
    }

    return ppw->pw_name;
}

/* caller must free returned string */
char *escape_percents(const char *string)
{
    char *escaped;
    size_t length;
    size_t spos, epos;

    if (string == NULL) {
        return NULL;
    }

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

/* vim: set ts=4 sw=4 tw=0 et:*/
