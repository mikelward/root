#include <assert.h>
#include <stdio.h>
#include <unistd.h>

#include "user.h"

void test_in_group_matches_primary_gid(void)
{
    printf("Running %s\n", __func__);
    assert(in_group(getgid()));
}

void test_become_user_non_root_rejected(void)
{
    printf("Running %s\n", __func__);
    assert(!become_user(1));
}

int main(void)
{
    test_in_group_matches_primary_gid();
    test_become_user_non_root_rejected();
    return 0;
}

/* vim: set ts=4 sw=4 tw=0 et:*/
