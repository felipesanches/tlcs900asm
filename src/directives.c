/*
 * TLCS-900 Assembler - Directive Handlers
 *
 * Handles ASL-compatible directives:
 * - ORG: Set program counter
 * - EQU/SET: Define constants
 * - DB/DW/DD: Define data
 * - DS/ALIGN: Reserve space
 * - INCLUDE/BINCLUDE: File inclusion
 * - CPU/MAXMODE: Processor settings
 * - IF/ELSE/ENDIF: Conditional assembly
 * - MACRO/ENDM: Macro definitions
 */

#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include "../include/tlcs900.h"

/* External functions */
extern bool assembler_include_file(Assembler *as, const char *filename);
extern void output_set_base(Assembler *as, uint32_t base);
extern void emit_fill(Assembler *as, size_t count, uint8_t value);
extern void emit_string(Assembler *as, const char *str, size_t len);
extern void emit_word24(Assembler *as, uint32_t w);

/* Macro functions */
extern bool macro_start_definition(Assembler *as, const char *name, const char *params_str);
extern bool macro_end_definition(Assembler *as);
extern bool macro_is_collecting(void);

/* Check if a string matches a directive (case-insensitive) */
static bool is_directive(const char *s, const char *directive) {
    return strcasecmp(s, directive) == 0;
}

/* Parse a quoted string, returning allocated string */
static char *parse_string_arg(void) {
    Token tok = lexer_peek();
    if (tok.type == TOK_STRING) {
        lexer_next();
        return strdup(tok.text);
    }
    if (tok.type == TOK_CHAR) {
        lexer_next();
        return strdup(tok.text);
    }
    /* Unquoted - collect until comma or end */
    char buf[MAX_IDENTIFIER];
    int i = 0;
    while (tok.type != TOK_COMMA && tok.type != TOK_NEWLINE && tok.type != TOK_EOF) {
        if (tok.type == TOK_IDENTIFIER || tok.type == TOK_NUMBER) {
            size_t len = strlen(tok.text);
            if (i + len < sizeof(buf) - 1) {
                strcpy(buf + i, tok.text);
                i += len;
            }
        }
        lexer_next();
        tok = lexer_peek();
    }
    buf[i] = '\0';
    return strdup(buf);
}

/* Handle ORG directive */
static bool handle_org(Assembler *as) {
    int64_t value;
    bool known;
    if (!expr_parse(as, &value, &known)) {
        error(as, "invalid ORG expression");
        return false;
    }
    if (!known && as->pass == 2) {
        error(as, "ORG value must be known in pass 1");
        return false;
    }
    as->pc = (uint32_t)value;
    as->org = (uint32_t)value;
    output_set_base(as, as->org);
    return true;
}

/* Handle EQU directive (label EQU value) */
static bool handle_equ(Assembler *as, const char *label) {
    if (!label || !label[0]) {
        error(as, "EQU requires a label");
        return false;
    }
    int64_t value;
    bool known;
    if (!expr_parse(as, &value, &known)) {
        error(as, "invalid EQU expression");
        return false;
    }
    symbol_define(as, label, SYM_EQU, value);
    return true;
}

/* Handle SET directive (reassignable) */
static bool handle_set(Assembler *as, const char *label) {
    if (!label || !label[0]) {
        error(as, "SET requires a label");
        return false;
    }
    int64_t value;
    bool known;
    if (!expr_parse(as, &value, &known)) {
        error(as, "invalid SET expression");
        return false;
    }
    symbol_define(as, label, SYM_SET, value);
    return true;
}

/* Handle DB (define byte) directive */
static bool handle_db(Assembler *as) {
    do {
        Token tok = lexer_peek();

        if (tok.type == TOK_STRING) {
            /* String literal - emit each byte */
            lexer_next();
            emit_string(as, tok.text, strlen(tok.text));
        } else if (tok.type == TOK_CHAR) {
            /* Character literal */
            lexer_next();
            emit_string(as, tok.text, strlen(tok.text));
        } else {
            /* Expression */
            int64_t value;
            bool known;
            if (!expr_parse(as, &value, &known)) {
                error(as, "invalid DB expression");
                return false;
            }
            emit_byte(as, (uint8_t)value);
        }

        /* Check for comma */
        tok = lexer_peek();
        if (tok.type == TOK_COMMA) {
            lexer_next();
        } else {
            break;
        }
    } while (true);

    return true;
}

/* Handle DW (define word) directive */
static bool handle_dw(Assembler *as) {
    do {
        int64_t value;
        bool known;
        if (!expr_parse(as, &value, &known)) {
            error(as, "invalid DW expression");
            return false;
        }
        emit_word(as, (uint16_t)value);

        /* Check for comma */
        Token tok = lexer_peek();
        if (tok.type == TOK_COMMA) {
            lexer_next();
        } else {
            break;
        }
    } while (true);

    return true;
}

/* Handle DD (define double/long) directive */
static bool handle_dd(Assembler *as) {
    do {
        int64_t value;
        bool known;
        if (!expr_parse(as, &value, &known)) {
            error(as, "invalid DD expression");
            return false;
        }
        emit_long(as, (uint32_t)value);

        /* Check for comma */
        Token tok = lexer_peek();
        if (tok.type == TOK_COMMA) {
            lexer_next();
        } else {
            break;
        }
    } while (true);

    return true;
}

/* Handle DS (define space) directive */
static bool handle_ds(Assembler *as) {
    int64_t count;
    bool known;
    if (!expr_parse(as, &count, &known)) {
        error(as, "invalid DS expression");
        return false;
    }

    uint8_t fill = 0;
    Token tok = lexer_peek();
    if (tok.type == TOK_COMMA) {
        lexer_next();
        int64_t fill_val;
        if (!expr_parse(as, &fill_val, &known)) {
            error(as, "invalid DS fill value");
            return false;
        }
        fill = (uint8_t)fill_val;
    }

    emit_fill(as, (size_t)count, fill);
    return true;
}

/* Handle ALIGN directive */
static bool handle_align(Assembler *as) {
    int64_t boundary;
    bool known;
    if (!expr_parse(as, &boundary, &known)) {
        error(as, "invalid ALIGN expression");
        return false;
    }

    if (boundary <= 0 || (boundary & (boundary - 1)) != 0) {
        error(as, "ALIGN boundary must be a power of 2");
        return false;
    }

    uint32_t mask = (uint32_t)boundary - 1;
    uint32_t padding = (boundary - (as->pc & mask)) & mask;
    emit_fill(as, padding, 0);
    return true;
}

/* Handle INCLUDE directive */
static bool handle_include(Assembler *as) {
    Token tok = lexer_peek();
    char *filename = NULL;

    if (tok.type == TOK_STRING || tok.type == TOK_CHAR) {
        lexer_next();
        filename = strdup(tok.text);
    } else if (tok.type == TOK_IDENTIFIER) {
        /* Unquoted filename */
        filename = parse_string_arg();
    } else {
        error(as, "INCLUDE requires a filename");
        return false;
    }

    bool result = assembler_include_file(as, filename);
    free(filename);
    return result;
}

/* Handle BINCLUDE directive (binary include) */
static bool handle_binclude(Assembler *as) {
    Token tok = lexer_peek();
    char *filename = NULL;

    if (tok.type == TOK_STRING || tok.type == TOK_CHAR) {
        lexer_next();
        filename = strdup(tok.text);
    } else {
        filename = parse_string_arg();
    }

    /* Optional offset and length */
    int64_t offset = 0;
    int64_t length = -1;
    bool known;

    tok = lexer_peek();
    if (tok.type == TOK_COMMA) {
        lexer_next();
        if (!expr_parse(as, &offset, &known)) {
            free(filename);
            error(as, "invalid BINCLUDE offset");
            return false;
        }

        tok = lexer_peek();
        if (tok.type == TOK_COMMA) {
            lexer_next();
            if (!expr_parse(as, &length, &known)) {
                free(filename);
                error(as, "invalid BINCLUDE length");
                return false;
            }
        }
    }

    /* Resolve path relative to current file */
    char resolved_path[1024];
    if (filename[0] != '/') {
        const char *last_slash = strrchr(as->current_file, '/');
        if (last_slash) {
            size_t dir_len = last_slash - as->current_file + 1;
            if (dir_len + strlen(filename) >= sizeof(resolved_path)) {
                free(filename);
                error(as, "BINCLUDE path too long");
                return false;
            }
            memcpy(resolved_path, as->current_file, dir_len);
            strcpy(resolved_path + dir_len, filename);
        } else {
            strncpy(resolved_path, filename, sizeof(resolved_path) - 1);
        }
    } else {
        strncpy(resolved_path, filename, sizeof(resolved_path) - 1);
    }
    resolved_path[sizeof(resolved_path) - 1] = '\0';
    free(filename);

    FILE *fp = fopen(resolved_path, "rb");
    if (!fp) {
        error(as, "cannot open binary file '%s'", resolved_path);
        return false;
    }

    /* Get file size */
    fseek(fp, 0, SEEK_END);
    long file_size = ftell(fp);

    if (offset >= file_size) {
        fclose(fp);
        error(as, "BINCLUDE offset beyond file size");
        return false;
    }

    if (length < 0) {
        length = file_size - offset;
    }

    if (offset + length > file_size) {
        length = file_size - offset;
    }

    fseek(fp, offset, SEEK_SET);

    /* Read and emit bytes */
    for (long i = 0; i < length; i++) {
        int c = fgetc(fp);
        if (c == EOF) break;
        emit_byte(as, (uint8_t)c);
    }

    fclose(fp);
    return true;
}

/* Handle CPU directive */
static bool handle_cpu(Assembler *as) {
    Token tok = lexer_next();
    if (tok.type != TOK_IDENTIFIER) {
        error(as, "CPU requires a processor name");
        return false;
    }

    /* Accept various TLCS-900 variants */
    if (strcasecmp(tok.text, "TLCS900") == 0 ||
        strcasecmp(tok.text, "TMP94C241") == 0 ||
        strcasecmp(tok.text, "TLCS-900") == 0 ||
        strcasecmp(tok.text, "TLCS900H") == 0 ||
        strcasecmp(tok.text, "TLCS900/H") == 0 ||
        strncasecmp(tok.text, "900", 3) == 0) {
        /* OK */
        return true;
    }

    warning(as, "unknown CPU '%s', assuming TLCS-900", tok.text);
    return true;
}

/* Handle MAXMODE directive */
static bool handle_maxmode(Assembler *as) {
    Token tok = lexer_peek();
    if (tok.type == TOK_IDENTIFIER) {
        lexer_next();
        if (strcasecmp(tok.text, "ON") == 0) {
            as->max_mode = true;
        } else if (strcasecmp(tok.text, "OFF") == 0) {
            as->max_mode = false;
        } else {
            error(as, "MAXMODE expects ON or OFF");
            return false;
        }
    } else {
        /* Default to ON */
        as->max_mode = true;
    }
    return true;
}

/* Handle END directive */
static bool handle_end(Assembler *as) {
    /* Optional start address - we ignore it for now */
    (void)as;
    return true;
}

/* Handle PAGE/NEWPAGE directive (listing control - ignored) */
static bool handle_page(Assembler *as) {
    (void)as;
    /* Skip to end of line */
    while (lexer_peek().type != TOK_NEWLINE && lexer_peek().type != TOK_EOF) {
        lexer_next();
    }
    return true;
}

/* Handle MACRO directive (label MACRO params) */
static bool handle_macro(Assembler *as, const char *label) {
    if (!label || !label[0]) {
        error(as, "MACRO requires a name (label)");
        return false;
    }

    /* Collect the rest of the line as parameter list */
    char params[MAX_LINE_LENGTH] = "";
    int pos = 0;
    Token tok = lexer_peek();
    while (tok.type != TOK_NEWLINE && tok.type != TOK_EOF) {
        if (pos > 0 && pos < (int)sizeof(params) - 1) {
            params[pos++] = ' ';
        }
        size_t len = strlen(tok.text);
        if (pos + len < sizeof(params) - 1) {
            strcpy(params + pos, tok.text);
            pos += len;
        }
        lexer_next();
        tok = lexer_peek();
    }
    params[pos] = '\0';

    return macro_start_definition(as, label, params);
}

/* Handle ENDM directive */
static bool handle_endm(Assembler *as) {
    return macro_end_definition(as);
}

/* Check and handle a directive, return true if it was a directive */
bool handle_directive(Assembler *as, const char *directive, const char *label) {
    if (is_directive(directive, "ORG")) {
        return handle_org(as);
    }
    if (is_directive(directive, "EQU") || is_directive(directive, "=")) {
        return handle_equ(as, label);
    }
    if (is_directive(directive, "SET")) {
        return handle_set(as, label);
    }
    if (is_directive(directive, "DB") || is_directive(directive, "DEFB") ||
        is_directive(directive, "DC.B") || is_directive(directive, "FCB") ||
        is_directive(directive, "BYT") || is_directive(directive, ".BYTE")) {
        return handle_db(as);
    }
    if (is_directive(directive, "DW") || is_directive(directive, "DEFW") ||
        is_directive(directive, "DC.W") || is_directive(directive, "FDB") ||
        is_directive(directive, "WOR") || is_directive(directive, ".WORD") ||
        is_directive(directive, "DATA")) {
        return handle_dw(as);
    }
    if (is_directive(directive, "DD") || is_directive(directive, "DEFL") ||
        is_directive(directive, "DC.L") || is_directive(directive, ".LONG")) {
        return handle_dd(as);
    }
    if (is_directive(directive, "DS") || is_directive(directive, "DEFS") ||
        is_directive(directive, "RMB") || is_directive(directive, "RES") ||
        is_directive(directive, ".BLKB")) {
        return handle_ds(as);
    }
    if (is_directive(directive, "ALIGN")) {
        return handle_align(as);
    }
    if (is_directive(directive, "INCLUDE")) {
        return handle_include(as);
    }
    if (is_directive(directive, "BINCLUDE") || is_directive(directive, "INCBIN")) {
        return handle_binclude(as);
    }
    if (is_directive(directive, "CPU") || is_directive(directive, ".CPU")) {
        return handle_cpu(as);
    }
    if (is_directive(directive, "MAXMODE")) {
        return handle_maxmode(as);
    }
    if (is_directive(directive, "END")) {
        return handle_end(as);
    }
    if (is_directive(directive, "PAGE") || is_directive(directive, "NEWPAGE")) {
        return handle_page(as);
    }
    if (is_directive(directive, "LISTING") || is_directive(directive, "PRTINIT") ||
        is_directive(directive, "PRTEXIT")) {
        /* Listing control - ignored */
        while (lexer_peek().type != TOK_NEWLINE && lexer_peek().type != TOK_EOF) {
            lexer_next();
        }
        return true;
    }
    if (is_directive(directive, "MACRO")) {
        return handle_macro(as, label);
    }
    if (is_directive(directive, "ENDM")) {
        return handle_endm(as);
    }

    return false;  /* Not a directive */
}
