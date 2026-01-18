/*
 * TLCS-900 Assembler - Macro Processor
 *
 * Handles MACRO/ENDM definitions and macro expansion.
 *
 * Macro syntax (ASL-compatible):
 *   NAME MACRO [param1, param2, ...]
 *     body lines
 *   ENDM
 *
 * Macro invocation:
 *   NAME [arg1, arg2, ...]
 */

#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include "../include/tlcs900.h"

/* External symbol functions */
extern Symbol *symbol_define_macro(Assembler *as, const char *name,
                                   char **params, int param_count,
                                   char **body, int body_count);
extern Symbol *symbol_lookup(Assembler *as, const char *name);

/* Macro expansion state */
static struct {
    char **lines;           /* Expanded lines to process */
    int line_count;
    int current_line;
    char *params[MAX_MACRO_PARAMS];
    char *args[MAX_MACRO_PARAMS];
    int param_count;
    int arg_count;
} macro_expansion[MAX_MACRO_DEPTH];

static int macro_depth = 0;

/* Check if currently collecting a macro definition */
static bool collecting_macro = false;
static char macro_name[MAX_IDENTIFIER];
static char *macro_params[MAX_MACRO_PARAMS];
static int macro_param_count = 0;
static char **macro_body = NULL;
static int macro_body_count = 0;
static int macro_body_capacity = 0;

/* Start collecting a macro definition */
bool macro_start_definition(Assembler *as, const char *name, const char *params_str) {
    if (collecting_macro) {
        error(as, "nested macro definitions not allowed");
        return false;
    }

    collecting_macro = true;
    strncpy(macro_name, name, MAX_IDENTIFIER - 1);
    macro_name[MAX_IDENTIFIER - 1] = '\0';

    /* Parse parameters */
    macro_param_count = 0;
    if (params_str && *params_str) {
        char params_copy[MAX_LINE_LENGTH];
        strncpy(params_copy, params_str, sizeof(params_copy) - 1);
        params_copy[sizeof(params_copy) - 1] = '\0';

        char *p = params_copy;
        while (*p && macro_param_count < MAX_MACRO_PARAMS) {
            /* Skip whitespace */
            while (*p == ' ' || *p == '\t' || *p == ',') p++;
            if (!*p) break;

            /* Extract parameter name */
            char *start = p;
            while (*p && *p != ' ' && *p != '\t' && *p != ',' && *p != '\n') p++;

            size_t len = p - start;
            if (len > 0) {
                macro_params[macro_param_count] = malloc(len + 1);
                memcpy(macro_params[macro_param_count], start, len);
                macro_params[macro_param_count][len] = '\0';
                macro_param_count++;
            }
        }
    }

    /* Initialize body storage */
    macro_body_count = 0;
    macro_body_capacity = 16;
    macro_body = malloc(macro_body_capacity * sizeof(char *));

    return true;
}

/* Add a line to the current macro definition */
bool macro_add_line(const char *line) {
    if (!collecting_macro) return false;

    /* Check for ENDM */
    const char *p = line;
    while (*p == ' ' || *p == '\t') p++;
    if (strncasecmp(p, "ENDM", 4) == 0) {
        char c = p[4];
        if (c == '\0' || c == ' ' || c == '\t' || c == '\n' || c == ';') {
            return false;  /* Signal end of macro */
        }
    }

    /* Grow buffer if needed */
    if (macro_body_count >= macro_body_capacity) {
        macro_body_capacity *= 2;
        macro_body = realloc(macro_body, macro_body_capacity * sizeof(char *));
    }

    macro_body[macro_body_count++] = strdup(line);
    return true;
}

/* Finish macro definition and store in symbol table */
bool macro_end_definition(Assembler *as) {
    if (!collecting_macro) {
        error(as, "ENDM without MACRO");
        return false;
    }

    /* Store in symbol table */
    Symbol *sym = symbol_define_macro(as, macro_name,
                                       macro_params, macro_param_count,
                                       macro_body, macro_body_count);

    /* Clean up temporary storage (symbol table now owns the data) */
    for (int i = 0; i < macro_param_count; i++) {
        free(macro_params[i]);
        macro_params[i] = NULL;
    }
    /* Note: macro_body strings are now owned by symbol table */
    free(macro_body);
    macro_body = NULL;

    collecting_macro = false;
    return sym != NULL;
}

/* Check if currently collecting a macro */
bool macro_is_collecting(void) {
    return collecting_macro;
}

/* Check if a symbol is a macro and get its definition */
Symbol *macro_lookup(Assembler *as, const char *name) {
    Symbol *sym = symbol_lookup(as, name);
    if (sym && sym->type == SYM_MACRO) {
        return sym;
    }
    return NULL;
}

/* Substitute parameters in a line */
static char *substitute_params(const char *line, Symbol *macro, char **args, int arg_count) {
    char *result = malloc(MAX_LINE_LENGTH);
    char *out = result;
    const char *in = line;

    while (*in && (out - result) < MAX_LINE_LENGTH - 1) {
        /* Check for parameter reference */
        bool found = false;
        for (int i = 0; i < macro->macro_param_count && i < arg_count; i++) {
            size_t plen = strlen(macro->macro_params[i]);
            if (strncasecmp(in, macro->macro_params[i], plen) == 0) {
                /* Check it's not part of a larger identifier */
                char next = in[plen];
                char prev = (in > line) ? in[-1] : ' ';
                if (!isalnum(prev) && prev != '_' &&
                    !isalnum(next) && next != '_') {
                    /* Substitute */
                    size_t alen = strlen(args[i]);
                    if ((out - result) + alen < MAX_LINE_LENGTH - 1) {
                        strcpy(out, args[i]);
                        out += alen;
                        in += plen;
                        found = true;
                        break;
                    }
                }
            }
        }
        if (!found) {
            *out++ = *in++;
        }
    }
    *out = '\0';
    return result;
}

/* Parse macro arguments from the rest of the line */
static int parse_macro_args(const char *args_str, char **args, int max_args) {
    int count = 0;
    if (!args_str || !*args_str) return 0;

    char args_copy[MAX_LINE_LENGTH];
    strncpy(args_copy, args_str, sizeof(args_copy) - 1);
    args_copy[sizeof(args_copy) - 1] = '\0';

    char *p = args_copy;
    while (*p && count < max_args) {
        /* Skip whitespace */
        while (*p == ' ' || *p == '\t') p++;
        if (!*p || *p == ';' || *p == '\n') break;

        /* Extract argument (handle parentheses for expressions) */
        char *start = p;
        int paren_depth = 0;
        while (*p && (paren_depth > 0 || (*p != ',' && *p != ';' && *p != '\n'))) {
            if (*p == '(') paren_depth++;
            else if (*p == ')') paren_depth--;
            p++;
        }

        /* Trim trailing whitespace */
        char *end = p;
        while (end > start && (end[-1] == ' ' || end[-1] == '\t')) end--;

        size_t len = end - start;
        if (len > 0) {
            args[count] = malloc(len + 1);
            memcpy(args[count], start, len);
            args[count][len] = '\0';
            count++;
        }

        if (*p == ',') p++;
    }

    return count;
}

/* Expand a macro invocation */
bool macro_expand(Assembler *as, Symbol *macro, const char *args_str) {
    if (macro_depth >= MAX_MACRO_DEPTH) {
        error(as, "macro expansion too deep");
        return false;
    }

    /* Parse arguments */
    char *args[MAX_MACRO_PARAMS];
    int arg_count = parse_macro_args(args_str, args, MAX_MACRO_PARAMS);

    /* Check argument count */
    if (arg_count < macro->macro_param_count) {
        /* Fill missing args with empty strings */
        for (int i = arg_count; i < macro->macro_param_count; i++) {
            args[i] = strdup("");
        }
        arg_count = macro->macro_param_count;
    }

    /* Process each line of the macro body */
    for (int i = 0; i < macro->macro_body_lines; i++) {
        char *expanded = substitute_params(macro->macro_body[i], macro, args, arg_count);

        /* Parse and execute the expanded line */
        /* Save current line context */
        int saved_line = as->current_line;

        /* Process the expanded line through parse_line */
        extern bool parse_line(Assembler *as, const char *line);
        parse_line(as, expanded);

        as->current_line = saved_line;
        free(expanded);
    }

    /* Free arguments */
    for (int i = 0; i < arg_count; i++) {
        free(args[i]);
    }

    return true;
}

/* Try to expand a potential macro invocation, return true if it was a macro */
bool macro_try_expand(Assembler *as, const char *name, const char *args_str) {
    Symbol *macro = macro_lookup(as, name);
    if (!macro) {
        return false;
    }
    return macro_expand(as, macro, args_str);
}
