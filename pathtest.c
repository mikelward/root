#define _DEFAULT_SOURCE /* for strdup(), glibc >= 2.20 */
#define _BSD_SOURCE     /* for strdup() */

#include <assert.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>

#include "path.h"

/*
 * Helpers for testing pathenv_each
 */
static int entry_count;
static char *entries[100];

void collect_entry(const char *entry)
{
    entries[entry_count++] = strdup(entry);
}

void reset_entries(void)
{
    for (int i = 0; i < entry_count; i++) {
        free(entries[i]);
        entries[i] = NULL;
    }
    entry_count = 0;
}

/*
 * pathenv_each tests
 */
void test_pathenv_each_basic(void)
{
    printf("Running %s\n", __func__);
    reset_entries();
    pathenv_each("/usr/bin:/bin", collect_entry);
    assert(entry_count == 2);
    assert(strcmp(entries[0], "/usr/bin") == 0);
    assert(strcmp(entries[1], "/bin") == 0);
    reset_entries();
}

void test_pathenv_each_single(void)
{
    printf("Running %s\n", __func__);
    reset_entries();
    pathenv_each("/usr/bin", collect_entry);
    assert(entry_count == 1);
    assert(strcmp(entries[0], "/usr/bin") == 0);
    reset_entries();
}

void test_pathenv_each_empty_start(void)
{
    printf("Running %s\n", __func__);
    reset_entries();
    pathenv_each(":/usr/bin", collect_entry);
    assert(entry_count == 2);
    assert(strcmp(entries[0], ".") == 0);
    assert(strcmp(entries[1], "/usr/bin") == 0);
    reset_entries();
}

void test_pathenv_each_empty_end(void)
{
    printf("Running %s\n", __func__);
    reset_entries();
    pathenv_each("/usr/bin:", collect_entry);
    assert(entry_count == 2);
    assert(strcmp(entries[0], "/usr/bin") == 0);
    assert(strcmp(entries[1], ".") == 0);
    reset_entries();
}

void test_pathenv_each_empty_middle(void)
{
    printf("Running %s\n", __func__);
    reset_entries();
    pathenv_each("/usr/bin::/bin", collect_entry);
    assert(entry_count == 3);
    assert(strcmp(entries[0], "/usr/bin") == 0);
    assert(strcmp(entries[1], ".") == 0);
    assert(strcmp(entries[2], "/bin") == 0);
    reset_entries();
}

void test_pathenv_each_all_empty(void)
{
    printf("Running %s\n", __func__);
    reset_entries();
    pathenv_each("::", collect_entry);
    assert(entry_count == 3);
    assert(strcmp(entries[0], ".") == 0);
    assert(strcmp(entries[1], ".") == 0);
    assert(strcmp(entries[2], ".") == 0);
    reset_entries();
}

/*
 * is_absolute_path tests
 */
void test_is_absolute_path(void)
{
    printf("Running %s\n", __func__);
    assert(is_absolute_path("/bin/ls"));
    assert(is_absolute_path("/"));
    assert(!is_absolute_path("bin/ls"));
    assert(!is_absolute_path("./ls"));
    assert(!is_absolute_path(""));
    assert(!is_absolute_path(NULL));
}

/*
 * is_qualified_path tests
 */
void test_is_qualified_path(void)
{
    printf("Running %s\n", __func__);
    assert(is_qualified_path("/bin/ls"));
    assert(is_qualified_path("./ls"));
    assert(is_qualified_path("../bin/ls"));
    assert(!is_qualified_path("ls"));
    assert(!is_qualified_path(NULL));
}

/*
 * is_unqualified_path tests
 */
void test_is_unqualified_path(void)
{
    printf("Running %s\n", __func__);
    assert(is_unqualified_path("ls"));
    assert(!is_unqualified_path("/bin/ls"));
    assert(!is_unqualified_path("./ls"));
    assert(!is_unqualified_path(NULL));
}

void test_get_command_path_rejects_empty_command(void)
{
    printf("Running %s\n", __func__);
    assert(get_command_path("", "/bin") == NULL);
    assert(get_command_path(NULL, "/bin") == NULL);
}

void test_get_command_path_ignores_directories(void)
{
    printf("Running %s\n", __func__);

    const char *tmpbase = getenv("TMPDIR");
    if (tmpbase == NULL || *tmpbase == '\0') {
        tmpbase = "/tmp";
    }

    char templ[PATH_MAX];
    int templ_length = snprintf(templ, sizeof(templ),
                                "%s/root-pathtest-XXXXXX", tmpbase);
    assert(templ_length > 0);
    assert((size_t)templ_length < sizeof(templ));

    char *tmpdir = mkdtemp(templ);
    assert(tmpdir != NULL);

    char commanddir[PATH_MAX];
    int length = snprintf(commanddir, sizeof(commanddir), "%s/fakecmd", tmpdir);
    assert(length > 0);
    assert((size_t)length < sizeof(commanddir));
    assert(mkdir(commanddir, 0700) == 0);

    char *path = get_command_path("fakecmd", tmpdir);
    assert(path == NULL);
    free(path);

    assert(rmdir(commanddir) == 0);
    assert(rmdir(tmpdir) == 0);
}

int main(int argc, const char *argv[])
{
    test_pathenv_each_basic();
    test_pathenv_each_single();
    test_pathenv_each_empty_start();
    test_pathenv_each_empty_end();
    test_pathenv_each_empty_middle();
    test_pathenv_each_all_empty();
    test_is_absolute_path();
    test_is_qualified_path();
    test_is_unqualified_path();
    test_get_command_path_rejects_empty_command();
    test_get_command_path_ignores_directories();

    return 0;
}

/* vim: set ts=4 sw=4 tw=0 et:*/
