/*
 * TLCS-900 Assembler - Error Reporting
 */

#include <stdio.h>
#include <stdarg.h>
#include "../include/tlcs900.h"

void error(Assembler *as, const char *fmt, ...) {
    va_list args;

    fprintf(stderr, "%s:%d: error: ",
            as->current_file ? as->current_file : "<input>",
            as->current_line);

    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);

    fprintf(stderr, "\n");

    as->errors = true;
    as->error_count++;
}

void warning(Assembler *as, const char *fmt, ...) {
    va_list args;

    fprintf(stderr, "%s:%d: warning: ",
            as->current_file ? as->current_file : "<input>",
            as->current_line);

    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);

    fprintf(stderr, "\n");

    as->warning_count++;
}
