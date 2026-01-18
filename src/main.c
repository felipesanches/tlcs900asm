/*
 * TLCS-900/TMP94C241 Assembler
 *
 * A dedicated assembler for the TLCS-900/H CPU family,
 * compatible with ASL (Alfred's Macro Assembler) syntax.
 *
 * Usage: tlcs900asm [options] input.asm -o output.rom
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include "../include/tlcs900.h"

static void print_usage(const char *progname) {
    fprintf(stderr, "TLCS-900/TMP94C241 Assembler v0.1\n\n");
    fprintf(stderr, "Usage: %s [options] input.asm\n\n", progname);
    fprintf(stderr, "Options:\n");
    fprintf(stderr, "  -o FILE    Output file (default: input.rom)\n");
    fprintf(stderr, "  -v         Verbose output\n");
    fprintf(stderr, "  -h         Show this help\n");
    fprintf(stderr, "\n");
}

int main(int argc, char *argv[]) {
    const char *input_file = NULL;
    const char *output_file = NULL;
    bool verbose = false;
    int opt;

    while ((opt = getopt(argc, argv, "o:vh")) != -1) {
        switch (opt) {
            case 'o':
                output_file = optarg;
                break;
            case 'v':
                verbose = true;
                break;
            case 'h':
                print_usage(argv[0]);
                return 0;
            default:
                print_usage(argv[0]);
                return 1;
        }
    }

    if (optind >= argc) {
        fprintf(stderr, "Error: no input file specified\n");
        print_usage(argv[0]);
        return 1;
    }

    input_file = argv[optind];

    /* Generate default output filename */
    char default_output[1024];
    if (!output_file) {
        strncpy(default_output, input_file, sizeof(default_output) - 5);
        char *dot = strrchr(default_output, '.');
        if (dot) {
            strcpy(dot, ".rom");
        } else {
            strcat(default_output, ".rom");
        }
        output_file = default_output;
    }

    /* Create assembler instance */
    Assembler *as = assembler_new();
    if (!as) {
        fprintf(stderr, "Error: failed to create assembler\n");
        return 1;
    }

    as->verbose = verbose;

    /* Assemble the file */
    if (!assembler_assemble_file(as, input_file)) {
        fprintf(stderr, "Assembly failed with %d errors\n", as->error_count);
        assembler_free(as);
        return 1;
    }

    /* Write output */
    if (!assembler_write_output(as, output_file)) {
        fprintf(stderr, "Failed to write output file\n");
        assembler_free(as);
        return 1;
    }

    if (verbose) {
        printf("Assembly successful: %s -> %s\n", input_file, output_file);
    }

    assembler_free(as);
    return 0;
}
