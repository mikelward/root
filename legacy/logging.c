#define _DEFAULT_SOURCE         /* for strdup(), glibc >= 2.20 */
#define _BSD_SOURCE             /* for strdup() */

#include <sys/types.h>
#include <errno.h>
#include <pwd.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>

#include "logging.h"

int loglevel = LOG_ERR;           /* only print ERROR, CRIT, ... */
static const char *g_progname;    /* XXX? maybe share this with root.o */
static char *g_username;          /* the caller's name, captured by initlog */

void setloglevel(int level)
{
    loglevel = level;
}

void initlog(const char *name)
{
    openlog(name, LOG_CONS|LOG_PID, LOG_AUTHPRIV);
    g_progname = strdup(name);
    if (g_progname == NULL) {
        fprintf(stderr, "root: Cannot allocate memory for program name\n");
    }

    /*
     * Capture the caller's name now, while the real uid is still theirs, so
     * messages logged after setuid(0) still name them. Copy it, because the
     * next getpwuid() overwrites the buffer it returns.
     */
    g_username = strdup(get_username(getuid()));
    if (g_username == NULL) {
        fprintf(stderr, "root: Cannot allocate memory for user name\n");
    }
}

/*
 * The calling user's name, as captured by initlog. Falls back to looking it
 * up afresh if initlog has not run or could not copy it.
 */
const char *log_username(void)
{
    if (g_username != NULL) {
        return g_username;
    }
    return get_username(getuid());
}

/*
 * If s starts with a well-formed UTF-8 sequence for a non-ASCII code point,
 * return its length (2 to 4) and store the code point in *cp; otherwise
 * return 0. Overlong forms, surrogates and code points past U+10FFFF are not
 * well formed (RFC 3629). Reading stops at the first byte that does not fit,
 * so this never reads past the terminating NUL.
 */
static size_t utf8_sequence(const unsigned char *s, unsigned long *cp)
{
    size_t length;
    unsigned long c;
    unsigned char low = 0x80, high = 0xBF;  /* allowed second byte */

    if (s[0] >= 0xC2 && s[0] <= 0xDF) {
        length = 2;
        c = s[0] & 0x1F;
    }
    else if (s[0] >= 0xE0 && s[0] <= 0xEF) {
        length = 3;
        c = s[0] & 0x0F;
        if (s[0] == 0xE0) {
            low = 0xA0;     /* no overlong forms */
        }
        else if (s[0] == 0xED) {
            high = 0x9F;    /* no surrogates */
        }
    }
    else if (s[0] >= 0xF0 && s[0] <= 0xF4) {
        length = 4;
        c = s[0] & 0x07;
        if (s[0] == 0xF0) {
            low = 0x90;     /* no overlong forms */
        }
        else if (s[0] == 0xF4) {
            high = 0x8F;    /* nothing past U+10FFFF */
        }
    }
    else {
        return 0;
    }

    if (s[1] < low || s[1] > high) {
        return 0;
    }
    c = (c << 6) | (s[1] & 0x3F);
    for (size_t i = 2; i < length; i++) {
        if (s[i] < 0x80 || s[i] > 0xBF) {
            return 0;
        }
        c = (c << 6) | (s[i] & 0x3F);
    }

    *cp = c;
    return length;
}

static size_t append_hex_escape(char *out, unsigned char byte)
{
    static const char digits[] = "0123456789abcdef";
    out[0] = '\\';
    out[1] = 'x';
    out[2] = digits[byte >> 4];
    out[3] = digits[byte & 0x0F];
    return 4;
}

static void escape_into(char *escaped, const char *string);

/*
 * Return a copy of string with outside text made safe for a message: a
 * backslash becomes "\\"; newline, carriage return and tab become "\n",
 * "\r" and "\t"; any other control character (U+0000 to U+001F, U+007F to
 * U+009F), and any byte that is not valid UTF-8, becomes "\xNN", one per
 * byte. All other valid UTF-8 passes through, so each message stays one
 * unambiguous line. The Rust build's logging::escape follows the same rules.
 *
 * Returns NULL if string is NULL or memory runs out.
 * Caller must free the returned string.
 */
char *escape_for_log(const char *string)
{
    if (string == NULL) {
        return NULL;
    }

    size_t length = strlen(string);
    if (length > (SIZE_MAX - 1) / 4) {
        return NULL;
    }
    /* Worst case: every byte becomes "\xNN". */
    char *escaped = malloc(length * 4 + 1);
    if (escaped == NULL) {
        return NULL;
    }

    escape_into(escaped, string);
    return escaped;
}

/*
 * Write string, escaped as escape_for_log() describes, into escaped, which
 * must hold ESCAPED_SIZE(strlen(string) + 1) bytes. Allocates nothing.
 */
static void escape_into(char *escaped, const char *string)
{
    const unsigned char *s = (const unsigned char *)string;
    size_t e = 0;
    while (*s != '\0') {
        unsigned long cp;
        size_t seqlen;

        if (*s == '\\' || *s == '\n' || *s == '\r' || *s == '\t') {
            escaped[e++] = '\\';
            escaped[e++] = *s == '\\' ? '\\' : *s == '\n' ? 'n'
                         : *s == '\r' ? 'r' : 't';
            s++;
        }
        else if (*s < 0x20 || *s == 0x7F) {
            e += append_hex_escape(escaped + e, *s++);
        }
        else if (*s < 0x80) {
            escaped[e++] = (char)*s++;
        }
        else if ((seqlen = utf8_sequence(s, &cp)) != 0) {
            /* U+0080 to U+009F are control characters too. */
            int control = cp <= 0x9F;
            for (size_t i = 0; i < seqlen; i++) {
                if (control) {
                    e += append_hex_escape(escaped + e, s[i]);
                }
                else {
                    escaped[e++] = (char)s[i];
                }
            }
            s += seqlen;
        }
        else {
            e += append_hex_escape(escaped + e, *s++);
        }
    }
    escaped[e] = '\0';
}

/*
 * Format a message and escape it with escape_for_log(), so no outside text
 * reaches the log or the terminal raw.
 *
 * Returns NULL if memory runs out. Caller must free the returned string.
 */
static char *format_escaped(const char *format, va_list ap)
{
    va_list aq;
    va_copy(aq, ap);
    int length = vsnprintf(NULL, 0, format, aq);
    va_end(aq);
    if (length < 0) {
        return NULL;
    }

    char *message = malloc((size_t)length + 1);
    if (message == NULL) {
        return NULL;
    }
    vsnprintf(message, (size_t)length + 1, format, ap);

    char *escaped = escape_for_log(message);
    free(message);
    return escaped;
}

void format_escaped_bounded(char *out, size_t max_raw,
                            const char *format, va_list ap)
{
    char raw[LOG_FALLBACK_MAX];
    if (max_raw > sizeof raw) {
        max_raw = sizeof raw;
    }
    vsnprintf(raw, max_raw, format, ap);
    escape_into(out, raw);
}

static void format_escaped_bounded_args(char *out, size_t max_raw,
                                        const char *format, ...)
{
    va_list ap;
    va_start(ap, format);
    format_escaped_bounded(out, max_raw, format, ap);
    va_end(ap);
}

/* Room for any user name: Linux allows 32 bytes, the BSDs fewer. */
#define USERNAME_MAX 256

void writelog(int priority, const char *format, va_list ap)
{
    va_list fallback_ap;
    va_copy(fallback_ap, ap);
    char *message = format_escaped(format, ap);
    char fallback[ESCAPED_SIZE(LOG_FALLBACK_MAX)];
    if (message == NULL) {
        /*
         * Out of memory: keep the arguments, so the record still names the
         * command, cut short if need be but still escaped.
         */
        format_escaped_bounded(fallback, LOG_FALLBACK_MAX, format, fallback_ap);
    }
    va_end(fallback_ap);

    char user[ESCAPED_SIZE(USERNAME_MAX)];
    format_escaped_bounded_args(user, USERNAME_MAX, "%s", log_username());

    /*
     * Only constant "%s" formats reach syslog, so no outside text is read as
     * a format.
     */
    syslog(priority, "%s: %s", user, message != NULL ? message : fallback);

    free(message);
}

void writescreen(int priority, const char *format, va_list ap)
{
    /* only print messages at loglevel or "lower" priority */
    /* with syslog, lowest means most important */
    if (priority > loglevel) {
        return;
    }

    va_list fallback_ap;
    va_copy(fallback_ap, ap);
    char *message = format_escaped(format, ap);
    char fallback[ESCAPED_SIZE(LOG_FALLBACK_MAX)];
    if (message == NULL) {
        /* As in writelog: out of memory, keep the arguments. */
        format_escaped_bounded(fallback, LOG_FALLBACK_MAX, format, fallback_ap);
    }
    va_end(fallback_ap);

    fprintf(stderr, "%s: %s\n",
            g_progname != NULL ? g_progname : "root",
            message != NULL ? message : fallback);
    free(message);
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

const char *get_username(uid_t uid)
{
    struct passwd *ppw = getpwuid(uid);
    if (ppw == NULL) {
        return "Unknown user";
    }

    return ppw->pw_name;
}

/* vim: set ts=4 sw=4 tw=0 et:*/
