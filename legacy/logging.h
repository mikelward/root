#ifndef LOGGING_H
#define LOGGING_H

extern int loglevel;

#include <sys/types.h>
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

/*
 * similar to info, error, etc., but only print to the screen
 */
void print(const char *format, ...);

const char *get_username(uid_t uid);

/*
 * The calling user's name for log messages: captured by initlog, so messages
 * logged after setuid(0) still name the caller rather than root.
 */
const char *log_username(void);

/*
 * Copy string with backslashes, control characters and invalid UTF-8
 * escaped, so outside text cannot forge or hide lines in a message.
 * Returns NULL if string is NULL or memory runs out; the caller frees it.
 */
char *escape_for_log(const char *string);

/*
 * Bytes needed for an escaped copy of up to n - 1 raw bytes, NUL included:
 * at worst every byte becomes "\xNN".
 */
#define ESCAPED_SIZE(n) (4 * ((n) - 1) + 1)

/*
 * The longest message, in raw bytes with its NUL, that writelog() and
 * writescreen() keep when memory runs out: room for a PATH_MAX path and
 * the text around it.
 */
#define LOG_FALLBACK_MAX (4096 + 256)

/*
 * Format a message into out and escape it as escape_for_log() does, without
 * allocating: the formatted message is first cut to max_raw - 1 bytes
 * (max_raw at most LOG_FALLBACK_MAX), and out must hold ESCAPED_SIZE(max_raw)
 * bytes. writelog() and writescreen() fall back to this when memory runs out,
 * so a record keeps its arguments.
 */
void format_escaped_bounded(char *out, size_t max_raw,
                            const char *format, va_list ap);

#endif
/* vim: set ts=4 sw=4 tw=0 et:*/
