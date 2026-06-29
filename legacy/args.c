#include "args.h"

#include <string.h>

int parse_args(int argc, const char *const *argv,
               struct options *opts, const char *const **argsp)
{
    opts->set_home = 1;
    opts->debug = 0;

    int i = 1; /* skip the program name */
    while (i < argc) {
        const char *arg = argv[i];

        /*
         * Stop at the first non-option argument (POSIX `+` semantics).
         * This covers an empty argument, an argument not starting with
         * '-', and a bare "-".
         */
        if (arg[0] != '-' || arg[1] == '\0') {
            break;
        }

        /* "--" terminates option processing and is consumed. */
        if (arg[1] == '-' && arg[2] == '\0') {
            i++;
            break;
        }

        if (arg[1] == '-') {
            /*
             * Long option. Only exact spellings are accepted; abbreviations
             * are rejected so a mistyped option cannot silently change the
             * behavior of a privileged command.
             */
            if (strcmp(arg, "--debug") == 0) {
                opts->debug = 1;
            }
            else if (strcmp(arg, "--home") == 0) {
                opts->set_home = 1;
            }
            else if (strcmp(arg, "--nohome") == 0) {
                opts->set_home = 0;
            }
            else {
                return -1;
            }
        }
        else {
            /* Short options, possibly combined (e.g. "-dH"). */
            for (const char *p = arg + 1; *p != '\0'; p++) {
                if (*p == 'd') {
                    opts->debug = 1;
                }
                else if (*p == 'H') {
                    opts->set_home = 0;
                }
                else {
                    return -1;
                }
            }
        }

        i++;
    }

    *argsp = argv + i;
    return 0;
}

/* vim: set ts=4 sw=4 tw=0 et:*/
