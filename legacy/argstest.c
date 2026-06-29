#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "args.h"

/*
 * Number of command-and-argument entries in the slice returned by parse_args.
 * rest points into argv, so the count is argc minus the parsed-option offset.
 */
static int rest_count(const char *const *argv, int argc, const char *const *rest)
{
    return argc - (int)(rest - argv);
}

void test_defaults(void)
{
    printf("Running %s\n", __func__);
    const char *const argv[] = {"root", "ls", NULL};
    struct options opts;
    const char *const *rest;
    assert(parse_args(2, argv, &opts, &rest) == 0);
    assert(opts.set_home == 1);
    assert(opts.debug == 0);
    assert(rest_count(argv, 2, rest) == 1);
    assert(strcmp(rest[0], "ls") == 0);
}

void test_debug_long_and_short(void)
{
    printf("Running %s\n", __func__);
    const char *const short_argv[] = {"root", "-d", "ls", NULL};
    const char *const long_argv[] = {"root", "--debug", "ls", NULL};
    struct options opts;
    const char *const *rest;

    assert(parse_args(3, short_argv, &opts, &rest) == 0);
    assert(opts.debug == 1);

    assert(parse_args(3, long_argv, &opts, &rest) == 0);
    assert(opts.debug == 1);
}

void test_nohome(void)
{
    printf("Running %s\n", __func__);
    const char *const short_argv[] = {"root", "-H", "ls", NULL};
    const char *const long_argv[] = {"root", "--nohome", "ls", NULL};
    struct options opts;
    const char *const *rest;

    assert(parse_args(3, short_argv, &opts, &rest) == 0);
    assert(opts.set_home == 0);

    assert(parse_args(3, long_argv, &opts, &rest) == 0);
    assert(opts.set_home == 0);
}

void test_home_overrides_nohome(void)
{
    printf("Running %s\n", __func__);
    const char *const argv[] = {"root", "-H", "--home", "ls", NULL};
    struct options opts;
    const char *const *rest;
    assert(parse_args(4, argv, &opts, &rest) == 0);
    assert(opts.set_home == 1);
}

void test_combined_short_options(void)
{
    printf("Running %s\n", __func__);
    const char *const argv[] = {"root", "-dH", "ls", NULL};
    struct options opts;
    const char *const *rest;
    assert(parse_args(3, argv, &opts, &rest) == 0);
    assert(opts.debug == 1);
    assert(opts.set_home == 0);
}

void test_stops_at_first_non_option(void)
{
    printf("Running %s\n", __func__);
    const char *const argv[] = {"root", "cmd", "-x", NULL};
    struct options opts;
    const char *const *rest;
    assert(parse_args(3, argv, &opts, &rest) == 0);
    assert(rest_count(argv, 3, rest) == 2);
    assert(strcmp(rest[0], "cmd") == 0);
    assert(strcmp(rest[1], "-x") == 0);
}

void test_double_dash_separator(void)
{
    printf("Running %s\n", __func__);
    const char *const argv[] = {"root", "--", "-d", NULL};
    struct options opts;
    const char *const *rest;
    assert(parse_args(3, argv, &opts, &rest) == 0);
    assert(opts.debug == 0);
    assert(rest_count(argv, 3, rest) == 1);
    assert(strcmp(rest[0], "-d") == 0);
}

void test_unknown_long_option(void)
{
    printf("Running %s\n", __func__);
    const char *const argv[] = {"root", "--bogus", NULL};
    struct options opts;
    const char *const *rest;
    assert(parse_args(2, argv, &opts, &rest) == -1);
}

void test_unknown_short_option(void)
{
    printf("Running %s\n", __func__);
    const char *const argv[] = {"root", "-x", NULL};
    struct options opts;
    const char *const *rest;
    assert(parse_args(2, argv, &opts, &rest) == -1);
}

void test_no_command(void)
{
    printf("Running %s\n", __func__);
    const char *const argv[] = {"root", "-d", NULL};
    struct options opts;
    const char *const *rest;
    assert(parse_args(2, argv, &opts, &rest) == 0);
    assert(rest_count(argv, 2, rest) == 0);
}

void test_rejects_abbreviated_long_options(void)
{
    printf("Running %s\n", __func__);
    const char *const deb[]   = {"root", "--deb", "ls", NULL};
    const char *const nohom[] = {"root", "--nohom", "ls", NULL};
    const char *const hom[]   = {"root", "--hom", "ls", NULL};
    struct options opts;
    const char *const *rest;

    assert(parse_args(3, deb, &opts, &rest) == -1);
    assert(parse_args(3, nohom, &opts, &rest) == -1);
    assert(parse_args(3, hom, &opts, &rest) == -1);
}

int main(int argc, const char *argv[])
{
    test_defaults();
    test_debug_long_and_short();
    test_nohome();
    test_home_overrides_nohome();
    test_combined_short_options();
    test_stops_at_first_non_option();
    test_double_dash_separator();
    test_unknown_long_option();
    test_unknown_short_option();
    test_no_command();
    test_rejects_abbreviated_long_options();

    return 0;
}

/* vim: set ts=4 sw=4 tw=0 et:*/
