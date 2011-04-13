#ifndef LOGGING_H
#define LOGGING_H

extern int loglevel;

#include <stdarg.h>
#include <pwd.h>

/*
 * call this before using logging
 */
void initlog(const char *name);
void setloglevel(int level);

/*
 * print messages when various types of events happen.
 * call it like printf(), do not use a trailing newline.
 */
void debug(const char *format, ...);
void error(const char *format, ...);
void info(const char *format, ...);

/*
 * helpers for above
 */
void writelog(int priority, const char *format, va_list ap);
void writescreen(int priority, const char *format, va_list ap);

char *get_username(uid_t uid);

char *escape_percents(const char *string);

#endif
/* vim: set ts=4 sw=4 tw=0 et:*/
