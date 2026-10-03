#define _DEFAULT_SOURCE /* for strdup(), glibc >= 2.20 */
#define _BSD_SOURCE     /* for strdup() */

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <syslog.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "logging.h"

void testescape1(void);
void testescape2(void);
void testescape3(void);
void testsetloglevel(void);
void testusernamesurvivessetuid(void);

int main(int argc, const char *argv[])
{
    testescape1();
    testescape2();
    testescape3();
    testsetloglevel();
    testusernamesurvivessetuid();

    return 0;
}

void testescape1(void)
{
    char *input = "mikel";
    char *expected = "mikel";
    char *actual;
    
    printf("Running %s\n", __func__);
    actual = escape_percents(input);

    assert(strcmp(actual, expected) == 0);
    free(actual);
}

void testescape2(void)
{
    char *input = NULL;
    char *actual;
    
    printf("Running %s\n", __func__);
    actual = escape_percents(input);

    assert(actual == NULL);
}

void testescape3(void)
{
    char *input = "%sally";
    char *expected = "%%sally";
    char *actual;
    
    printf("Running %s\n", __func__);
    actual = escape_percents(input);

    assert(strcmp(actual, expected) == 0);
    free(actual);
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
