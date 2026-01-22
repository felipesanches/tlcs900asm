/*
 * TLCS-900 Assembler - Main Assembler Control
 *
 * Two-pass assembler:
 * Pass 1: Collect labels and symbol values
 * Pass 2: Generate code with resolved symbols
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "../include/tlcs900.h"

/* External functions */
extern void output_init(Assembler *as);
extern void output_free(Assembler *as);
extern void output_set_base(Assembler *as, uint32_t base);
extern bool parse_line(Assembler *as, const char *line);

/* Create a new assembler instance */
Assembler *assembler_new(void) {
    Assembler *as = calloc(1, sizeof(Assembler));
    if (!as) {
        fprintf(stderr, "Failed to allocate assembler\n");
        return NULL;
    }

    symbols_init(as);
    output_init(as);

    as->pc = 0;
    as->org = 0;
    as->pass = 1;
    as->max_mode = true;  /* TLCS-900 typically runs in MAX mode */

    return as;
}

/* Free assembler instance */
void assembler_free(Assembler *as) {
    if (!as) return;

    symbols_free(as);
    output_free(as);

    /* Free include stack files */
    for (int i = 0; i < as->include_depth; i++) {
        if (as->include_stack[i].fp) {
            fclose(as->include_stack[i].fp);
        }
    }

    free(as);
}

/* Read a line from file, handling different line endings */
static char *read_line(FILE *fp, char *buf, size_t size) {
    if (!fgets(buf, size, fp)) {
        return NULL;
    }

    /* Remove trailing newline/carriage return */
    size_t len = strlen(buf);
    while (len > 0 && (buf[len-1] == '\n' || buf[len-1] == '\r')) {
        buf[--len] = '\0';
    }

    return buf;
}

/* Process a single file (used for includes too) */
static bool process_file(Assembler *as, const char *filename) {
    FILE *fp = fopen(filename, "r");
    if (!fp) {
        error(as, "cannot open file '%s'", filename);
        return false;
    }

    /* Save current file context */
    const char *prev_file = as->current_file;
    int prev_line = as->current_line;

    as->current_file = filename;
    as->current_line = 0;

    char line[MAX_LINE_LENGTH];
    while (read_line(fp, line, sizeof(line))) {
        as->current_line++;

        if (!parse_line(as, line)) {
            /* Error already reported */
        }

        /* Check for too many errors */
        if (as->error_count > 10000) {
            error(as, "too many errors, stopping");
            break;
        }
    }

    fclose(fp);

    /* Restore previous file context */
    as->current_file = prev_file;
    as->current_line = prev_line;

    return !as->errors;
}

/* Assemble a file (main entry point) */
bool assembler_assemble_file(Assembler *as, const char *filename) {
    /*
     * Multi-pass assembly to handle forward references correctly:
     *
     * Pass 1 (first iteration): Collect symbols, forward refs get max sizes
     * Pass 1 (iterations 2+): Recalculate with known values until stable
     * Pass 2: Generate code with final, stable sizes
     *
     * This iterative approach ensures that instruction sizes are consistent
     * and all labels have correct addresses for optimal sizing.
     */

    bool had_pass1_errors = false;
    uint32_t last_pc = 0;
    int iteration = 0;
    const int MAX_ITERATIONS = 10;

    /* Iterative pass 1: repeat until addresses stabilize */
    do {
        iteration++;
        if (as->verbose) {
            printf("Pass 1 (iteration %d): %s\n", iteration, filename);
        }

        as->pass = 1;
        as->sizing_pass = (iteration == 1);  /* Conservative only on first iteration */
        as->pc = 0;
        as->org = 0;
        as->errors = false;
        as->error_count = 0;

        if (!process_file(as, filename)) {
            return false;
        }

        if (as->errors) {
            had_pass1_errors = true;
        }

        /* Check if PC changed from last iteration */
        if (iteration > 1 && as->pc == last_pc) {
            if (as->verbose) {
                printf("  Sizes stabilized at iteration %d (PC=%u)\n", iteration, as->pc);
            }
            break;
        }

        last_pc = as->pc;

    } while (iteration < MAX_ITERATIONS);

    if (iteration >= MAX_ITERATIONS) {
        fprintf(stderr, "Warning: sizes did not stabilize after %d iterations\n", MAX_ITERATIONS);
    }

    if (had_pass1_errors) {
        fprintf(stderr, "Pass 1 had errors, continuing to pass 2...\n");
    }

    /* Pass 2: Generate code */
    if (as->verbose) {
        printf("Pass 2: %s\n", filename);
    }

    as->pass = 2;
    as->sizing_pass = false;
    as->pc = 0;
    as->org = 0;
    as->errors = false;
    as->error_count = 0;

    if (!process_file(as, filename)) {
        return false;
    }

    if (as->errors || had_pass1_errors) {
        fprintf(stderr, "Assembly failed with %d errors\n", as->error_count);
        /* Still output the file for debugging/comparison purposes */
        if (as->output_size > 0) {
            fprintf(stderr, "Partial output: %zu bytes generated (with errors)\n", as->output_size);
        }
        return false;
    }

    if (as->verbose) {
        printf("Assembly complete: %zu bytes generated\n", as->output_size);
    }

    return true;
}

/* Handle INCLUDE directive */
bool assembler_include_file(Assembler *as, const char *filename) {
    if (as->include_depth >= MAX_INCLUDE_DEPTH) {
        error(as, "include nesting too deep");
        return false;
    }

    /* Resolve relative path based on current file */
    char resolved_path[1024];
    if (filename[0] != '/') {
        /* Get directory of current file */
        const char *last_slash = strrchr(as->current_file, '/');
        if (last_slash) {
            size_t dir_len = last_slash - as->current_file + 1;
            if (dir_len + strlen(filename) >= sizeof(resolved_path)) {
                error(as, "include path too long");
                return false;
            }
            memcpy(resolved_path, as->current_file, dir_len);
            strcpy(resolved_path + dir_len, filename);
        } else {
            strncpy(resolved_path, filename, sizeof(resolved_path) - 1);
            resolved_path[sizeof(resolved_path) - 1] = '\0';
        }
    } else {
        strncpy(resolved_path, filename, sizeof(resolved_path) - 1);
        resolved_path[sizeof(resolved_path) - 1] = '\0';
    }

    return process_file(as, resolved_path);
}
