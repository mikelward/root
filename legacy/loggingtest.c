#define _DEFAULT_SOURCE /* for strdup(), glibc >= 2.20 */
#define _BSD_SOURCE     /* for strdup() */

#include <assert.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <syslog.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "logging.h"

void testescapeplain(void);
void testescapebackslashesandwhitespace(void);
void testescapecontrol(void);
void testescapeinvalidutf8(void);
void testescapebidiandlineseparators(void);
void testescapenull(void);
void testformatescapedbounded(void);
void testsetloglevel(void);
void testusernamefallsbacktouid(void);
void testusernamesurvivessetuid(void);

int main(int argc, const char *argv[])
{
    testescapeplain();
    testescapebackslashesandwhitespace();
    testescapecontrol();
    testescapeinvalidutf8();
    testescapebidiandlineseparators();
    testescapenull();
    testformatescapedbounded();
    testsetloglevel();
    testusernamefallsbacktouid();
    testusernamesurvivessetuid();

    return 0;
}

/*
 * The cases match the Rust build's escape tests in src/logging.rs, so the
 * two builds escape alike.
 */
static void checkescape(const char *input, const char *expected)
{
    char *actual = escape_for_log(input);
    assert(actual != NULL);
    if (strcmp(actual, expected) != 0) {
        fprintf(stderr, "escape_for_log: got \"%s\", want \"%s\"\n",
                actual, expected);
        assert(strcmp(actual, expected) == 0);
    }
    free(actual);
}

void testescapeplain(void)
{
    printf("Running %s\n", __func__);
    checkescape("/usr/bin/ls", "/usr/bin/ls");
    checkescape("/home/jos\xc3\xa9/bin", "/home/jos\xc3\xa9/bin");
    checkescape("", "");
    /* No longer passed as a format, so % needs no escaping. */
    checkescape("%sally", "%sally");
}

void testescapebackslashesandwhitespace(void)
{
    printf("Running %s\n", __func__);
    checkescape("a\\b", "a\\\\b");
    checkescape("a\nb\rc\td", "a\\nb\\rc\\td");
}

void testescapecontrol(void)
{
    printf("Running %s\n", __func__);
    checkescape("\x1b[2K", "\\x1b[2K");
    checkescape("\x01\x7f", "\\x01\\x7f");
    /* U+009B, the one-byte CSI some terminals honor, as UTF-8. */
    checkescape("\xc2\x9b", "\\xc2\\x9b");
}

void testescapeinvalidutf8(void)
{
    printf("Running %s\n", __func__);
    checkescape("\xff", "\\xff");
    /* A truncated sequence, an overlong form and a surrogate. */
    checkescape("\xe2\x82" "A", "\\xe2\\x82A");
    checkescape("\xc0\xaf", "\\xc0\\xaf");
    checkescape("\xed\xa0\x80", "\\xed\\xa0\\x80");
    /* A sequence cut short by the end of the string. */
    checkescape("\xf0\x9f\x98", "\\xf0\\x9f\\x98");
}

void testescapebidiandlineseparators(void)
{
    printf("Running %s\n", __func__);
    /* U+202E RIGHT-TO-LEFT OVERRIDE, which shows "txt.exe" as "exe.txt". */
    checkescape("a\xe2\x80\xaetxt.exe", "a\\xe2\\x80\\xaetxt.exe");
    /* The other bidi controls: ALM, LRM, RLM, LRE to RLO, LRI to PDI. */
    checkescape("\xd8\x9c", "\\xd8\\x9c");
    checkescape("\xe2\x80\x8e\xe2\x80\x8f", "\\xe2\\x80\\x8e\\xe2\\x80\\x8f");
    checkescape("\xe2\x80\xaa\xe2\x80\xad", "\\xe2\\x80\\xaa\\xe2\\x80\\xad");
    checkescape("\xe2\x81\xa6\xe2\x81\xa9", "\\xe2\\x81\\xa6\\xe2\\x81\\xa9");
    /* U+2028 LINE SEPARATOR and U+2029 PARAGRAPH SEPARATOR break lines. */
    checkescape("a\xe2\x80\xa8" "b\xe2\x80\xa9", "a\\xe2\\x80\\xa8" "b\\xe2\\x80\\xa9");
    /* Their neighbors pass through: U+200D ZWJ, U+202F, U+2065, U+206A. */
    checkescape("\xe2\x80\x8d\xe2\x80\xaf", "\xe2\x80\x8d\xe2\x80\xaf");
    checkescape("\xe2\x81\xa5\xe2\x81\xaa", "\xe2\x81\xa5\xe2\x81\xaa");
}

void testescapenull(void)
{
    printf("Running %s\n", __func__);
    assert(escape_for_log(NULL) == NULL);
}

static void bounded(char *out, size_t max_raw, const char *format, ...)
{
    va_list ap;
    va_start(ap, format);
    format_escaped_bounded(out, max_raw, format, ap);
    va_end(ap);
}

/*
 * The fallback writelog() uses when memory runs out must keep a record's
 * arguments, escaped, and never overrun its buffer.
 */
void testformatescapedbounded(void)
{
    printf("Running %s\n", __func__);
    char out[ESCAPED_SIZE(16)];

    bounded(out, 16, "Running %s", "a\nb");
    assert(strcmp(out, "Running a\\nb") == 0);

    /* Too long for the bound: cut to 15 bytes, then escaped. */
    bounded(out, 16, "Running %s", "/usr/local/bin/x");
    assert(strcmp(out, "Running /usr/lo") == 0);

    /* The worst case, every kept byte escaped, still fits. */
    bounded(out, 16, "%s", "\x01\x01\x01\x01\x01\x01\x01\x01"
                           "\x01\x01\x01\x01\x01\x01\x01\x01\x01");
    assert(strlen(out) == 15 * 4);
}

void testsetloglevel(void)
{
    printf("Running %s\n", __func__);

    /* default level is LOG_ERR */
    assert(loglevel == LOG_ERR);

    setloglevel(LOG_DEBUG);
    assert(loglevel == LOG_DEBUG);

    /* restore default */
    setloglevel(LOG_ERR);
    assert(loglevel == LOG_ERR);
}

/*
 * Messages logged after setuid(0) must still name the caller, so initlog has
 * to capture the real uid's name, and that name has to survive becoming
 * root. Changing uids is irreversible and needs root, so it happens in a
 * child process; CI reruns this suite under sudo so it does not skip there.
 */
/*
 * A caller with no user name to find is still named in the record, by uid.
 */
void testusernamefallsbacktouid(void)
{
    printf("Running %s\n", __func__);

    /* Any uid with no passwd entry will do. */
    uid_t unnamed = 2000000000;
    while (getpwuid(unnamed) != NULL) {
        unnamed++;
    }
    char expected[64];
    snprintf(expected, sizeof expected, "uid %lu", (unsigned long)unnamed);
    assert(strcmp(get_username(unnamed), expected) == 0);

    /* A uid with an entry keeps its name. */
    struct passwd *root = getpwuid(0);
    assert(root != NULL);
    char *name = strdup(root->pw_name);
    assert(name != NULL);
    assert(strcmp(get_username(0), name) == 0);
    free(name);

    /* And the name initlog captures for the record is the same "uid N". */
    if (geteuid() != 0) {
        printf("skipping the captured name: needs root to change uids in a child\n");
        return;
    }
    pid_t pid = fork();
    assert(pid != -1);
    if (pid == 0) {
        if (setreuid(unnamed, 0) != 0) {
            _exit(2);
        }
        initlog("roottest");
        _exit(strcmp(log_username(), expected) == 0 ? 0 : 1);
    }
    int status;
    assert(waitpid(pid, &status, 0) == pid);
    assert(WIFEXITED(status) && WEXITSTATUS(status) == 0);
}

void testusernamesurvivessetuid(void)
{
    printf("Running %s\n", __func__);
    if (geteuid() != 0) {
        printf("skipping: needs root to change uids in a child\n");
        return;
    }

    const uid_t caller = 65534;
    char *expected = strdup(get_username(caller));
    assert(expected != NULL);
    assert(strcmp(expected, get_username(0)) != 0);

    pid_t pid = fork();
    assert(pid != -1);
    if (pid == 0) {
        /*
         * Start where the installed setuid binary does: the real uid is the
         * caller's and the effective uid is root. Capturing the effective
         * uid would name root here.
         */
        if (setreuid(caller, 0) != 0) {
            _exit(2);
        }
        initlog("roottest");
        /*
         * Then become root the way become_root() does. Looking the name up
         * per message would name root from here on.
         */
        if (setuid(0) != 0) {
            _exit(3);
        }
        _exit(strcmp(log_username(), expected) == 0 ? 0 : 1);
    }

    int status;
    assert(waitpid(pid, &status, 0) == pid);
    assert(WIFEXITED(status) && WEXITSTATUS(status) == 0);
    free(expected);
}

/* vim: set ts=4 sw=4 tw=0 et:*/
