#include <assert.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "fds.h"

void testopensclosedfds(void);
void testleavesopenfdsalone(void);

int main(int argc, const char *argv[])
{
    testopensclosedfds();
    testleavesopenfdsalone();
    return 0;
}

/* Whether fd is open on /dev/null. */
static int is_dev_null(int fd)
{
    struct stat null, st;
    if (stat("/dev/null", &null) != 0 || fstat(fd, &st) != 0) {
        return 0;
    }
    return S_ISCHR(st.st_mode) && st.st_rdev == null.st_rdev;
}

/*
 * Run check in a child with the given standard descriptors closed, so this
 * process keeps its own. The child reports through its exit status.
 */
static void in_child(int close_mask, int (*check)(void))
{
    pid_t pid = fork();
    assert(pid != -1);
    if (pid == 0) {
        for (int fd = 0; fd <= 2; fd++) {
            if (close_mask & (1 << fd)) {
                close(fd);
            }
        }
        _exit(check());
    }
    int status;
    assert(waitpid(pid, &status, 0) == pid);
    assert(WIFEXITED(status));
    assert(WEXITSTATUS(status) == 0);
}

static int all_closed_become_dev_null(void)
{
    if (ensure_standard_fds_open() != 0) {
        return 1;
    }
    for (int fd = 0; fd <= 2; fd++) {
        if (!is_dev_null(fd)) {
            return 2 + fd;
        }
    }
    /* The next file opened must not land on a standard descriptor. */
    int next = open("/dev/null", O_RDONLY);
    return next > 2 ? 0 : 5;
}

void testopensclosedfds(void)
{
    in_child(07, all_closed_become_dev_null);
}

static int stderr_only_replaced(void)
{
    struct stat before0, before1;
    if (fstat(0, &before0) != 0 || fstat(1, &before1) != 0) {
        return 1;
    }
    if (ensure_standard_fds_open() != 0) {
        return 2;
    }
    struct stat after0, after1;
    if (fstat(0, &after0) != 0 || fstat(1, &after1) != 0) {
        return 3;
    }
    if (after0.st_ino != before0.st_ino || after0.st_dev != before0.st_dev
        || after1.st_ino != before1.st_ino || after1.st_dev != before1.st_dev) {
        return 4;
    }
    return is_dev_null(2) ? 0 : 5;
}

void testleavesopenfdsalone(void)
{
    in_child(04, stderr_only_replaced);
}

/* vim: set ts=4 sw=4 tw=0 et:*/
