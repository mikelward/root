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

    /* only print messages at loglevel or "lower" priority */
    /* with syslog, lowest means most important */
    if (priority > loglevel) {
        return;
    }

    progname = escape_percents(g_progname);

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
