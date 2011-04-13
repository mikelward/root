#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "logging.h"

void testescape1(void);
void testescape2(void);
void testescape3(void);

int main(int argc, const char *argv[])
{
    testescape1();
    testescape2();
    testescape3();

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
}

/* vim: set ts=4 sw=4 tw=0 et:*/
