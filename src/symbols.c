/*
 * TLCS-900 Assembler - Symbol Table
 *
 * Hash table implementation for labels, EQU constants, and macros.
 * Uses FNV-1a hash with chaining for collision resolution.
 */

#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include "../include/tlcs900.h"

#define SYMBOL_TABLE_SIZE 4096

/* FNV-1a hash function */
static uint32_t hash_string(const char *str) {
    uint32_t hash = 2166136261u;
    while (*str) {
        hash ^= (uint8_t)toupper(*str++);
        hash *= 16777619u;
    }
    return hash;
}

void symbols_init(Assembler *as) {
    as->symbol_table_size = SYMBOL_TABLE_SIZE;
    as->symbols = calloc(SYMBOL_TABLE_SIZE, sizeof(Symbol *));
    if (!as->symbols) {
        fprintf(stderr, "Failed to allocate symbol table\n");
        exit(1);
    }
}

void symbols_free(Assembler *as) {
    if (!as->symbols) return;

    for (size_t i = 0; i < as->symbol_table_size; i++) {
        Symbol *sym = as->symbols[i];
        while (sym) {
            Symbol *next = sym->next;

            /* Free macro body if present */
            if (sym->macro_body) {
                for (int j = 0; j < sym->macro_body_lines; j++) {
                    free(sym->macro_body[j]);
                }
                free(sym->macro_body);
            }

            /* Free macro params if present */
            if (sym->macro_params) {
                for (int j = 0; j < sym->macro_param_count; j++) {
                    free(sym->macro_params[j]);
                }
                free(sym->macro_params);
            }

            free(sym);
            sym = next;
        }
    }

    free(as->symbols);
    as->symbols = NULL;
}

Symbol *symbol_lookup(Assembler *as, const char *name) {
    uint32_t h = hash_string(name) % as->symbol_table_size;
    Symbol *sym = as->symbols[h];

    while (sym) {
        if (strcasecmp(sym->name, name) == 0) {
            return sym;
        }
        sym = sym->next;
    }

    return NULL;
}

Symbol *symbol_define(Assembler *as, const char *name, SymbolType type, int64_t value) {
    /* Check if already exists */
    Symbol *existing = symbol_lookup(as, name);
    if (existing) {
        if (existing->type == SYM_SET || type == SYM_SET) {
            /* SET symbols can be redefined */
            existing->value = value;
            existing->defined = true;
            existing->type = type;
            return existing;
        }
        if (existing->defined && as->pass == 1) {
            error(as, "symbol '%s' already defined at %s:%d",
                  name, existing->definition_file, existing->definition_line);
            return NULL;
        }
        /* Update value in pass 2 */
        existing->value = value;
        existing->defined = true;
        return existing;
    }

    /* Create new symbol */
    Symbol *sym = calloc(1, sizeof(Symbol));
    if (!sym) {
        error(as, "out of memory allocating symbol '%s'", name);
        return NULL;
    }

    strncpy(sym->name, name, MAX_IDENTIFIER - 1);
    sym->name[MAX_IDENTIFIER - 1] = '\0';
    sym->type = type;
    sym->value = value;
    sym->defined = true;
    sym->definition_line = as->current_line;
    sym->definition_file = as->current_file;

    /* Insert at head of chain */
    uint32_t h = hash_string(name) % as->symbol_table_size;
    sym->next = as->symbols[h];
    as->symbols[h] = sym;

    return sym;
}

bool symbol_is_defined(Assembler *as, const char *name) {
    Symbol *sym = symbol_lookup(as, name);
    return sym && sym->defined;
}

/* Get symbol value, marking it as referenced */
bool symbol_get_value(Assembler *as, const char *name, int64_t *value) {
    Symbol *sym = symbol_lookup(as, name);
    if (!sym) {
        return false;
    }
    sym->referenced = true;
    *value = sym->value;
    return sym->defined;
}

/* Define a macro */
Symbol *symbol_define_macro(Assembler *as, const char *name,
                            char **params, int param_count,
                            char **body, int body_lines) {
    Symbol *sym = symbol_define(as, name, SYM_MACRO, 0);
    if (!sym) return NULL;

    /* Allocate and copy parameters */
    if (param_count > 0) {
        sym->macro_params = malloc(param_count * sizeof(char *));
        sym->macro_param_count = param_count;
        for (int i = 0; i < param_count; i++) {
            sym->macro_params[i] = strdup(params[i]);
        }
    }

    /* Allocate and copy body */
    if (body_lines > 0) {
        sym->macro_body = malloc(body_lines * sizeof(char *));
        sym->macro_body_lines = body_lines;
        for (int i = 0; i < body_lines; i++) {
            sym->macro_body[i] = strdup(body[i]);
        }
    }

    return sym;
}

/* Dump symbol table for debugging */
void symbols_dump(Assembler *as) {
    printf("Symbol Table:\n");
    printf("%-32s %-8s %s\n", "Name", "Type", "Value");
    printf("%-32s %-8s %s\n", "----", "----", "-----");

    for (size_t i = 0; i < as->symbol_table_size; i++) {
        Symbol *sym = as->symbols[i];
        while (sym) {
            const char *type_str;
            switch (sym->type) {
                case SYM_LABEL: type_str = "LABEL"; break;
                case SYM_EQU: type_str = "EQU"; break;
                case SYM_SET: type_str = "SET"; break;
                case SYM_MACRO: type_str = "MACRO"; break;
                case SYM_SECTION: type_str = "SECTION"; break;
                default: type_str = "?"; break;
            }
            printf("%-32s %-8s $%08lX\n", sym->name, type_str, (unsigned long)sym->value);
            sym = sym->next;
        }
    }
}
