/*
 * TLCS-900 Assembler - Lexer (Tokenizer)
 *
 * Tokenizes ASL-style assembly syntax including:
 * - Identifiers (labels, mnemonics, register names)
 * - Numbers (decimal, hex with $ or 0x prefix, binary with %)
 * - Strings ("..." and '...')
 * - Operators and punctuation
 * - Comments (; to end of line)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "../include/tlcs900.h"

/* Lexer state */
static const char *input_start;
static const char *input_pos;
static int current_line;
static int current_column;
static Token peeked_token;
static bool has_peeked;

void lexer_init(const char *input) {
    input_start = input;
    input_pos = input;
    current_line = 1;
    current_column = 1;
    has_peeked = false;
}

void lexer_set_line(int line) {
    current_line = line;
}

static char peek_char(void) {
    return *input_pos;
}

static char next_char(void) {
    char c = *input_pos;
    if (c != '\0') {
        input_pos++;
        if (c == '\n') {
            current_line++;
            current_column = 1;
        } else {
            current_column++;
        }
    }
    return c;
}

static void skip_whitespace(void) {
    while (*input_pos == ' ' || *input_pos == '\t' || *input_pos == '\r') {
        next_char();
    }
}

static void skip_comment(void) {
    /* Skip from ; to end of line */
    while (*input_pos != '\0' && *input_pos != '\n') {
        next_char();
    }
}

static bool is_ident_start(char c) {
    return isalpha(c) || c == '_' || c == '.';
}

static bool is_ident_char(char c) {
    return isalnum(c) || c == '_' || c == '.';
}

static int64_t parse_hex(void) {
    int64_t value = 0;
    while (isxdigit(peek_char())) {
        char c = next_char();
        int digit;
        if (c >= '0' && c <= '9') {
            digit = c - '0';
        } else if (c >= 'A' && c <= 'F') {
            digit = c - 'A' + 10;
        } else {
            digit = c - 'a' + 10;
        }
        value = (value << 4) | digit;
    }
    /* Skip optional 'H' suffix */
    if (peek_char() == 'H' || peek_char() == 'h') {
        next_char();
    }
    return value;
}

static int64_t parse_binary(void) {
    int64_t value = 0;
    while (peek_char() == '0' || peek_char() == '1') {
        value = (value << 1) | (next_char() - '0');
    }
    return value;
}

static int64_t parse_decimal(void) {
    int64_t value = 0;
    while (isdigit(peek_char())) {
        value = value * 10 + (next_char() - '0');
    }
    return value;
}

static Token make_token(TokenType type) {
    Token tok;
    tok.type = type;
    tok.text[0] = '\0';
    tok.value = 0;
    tok.line = current_line;
    tok.column = current_column;
    return tok;
}

Token lexer_next(void) {
    if (has_peeked) {
        has_peeked = false;
        return peeked_token;
    }

    skip_whitespace();

    Token tok = make_token(TOK_EOF);
    tok.line = current_line;
    tok.column = current_column;

    char c = peek_char();

    if (c == '\0') {
        return tok;
    }

    /* Newline */
    if (c == '\n') {
        next_char();
        tok.type = TOK_NEWLINE;
        return tok;
    }

    /* Comment - treat as end of line */
    if (c == ';') {
        skip_comment();
        /* Return newline if there is one, otherwise EOF */
        if (peek_char() == '\n') {
            next_char();
            tok.type = TOK_NEWLINE;
        }
        return tok;
    }

    /* String literal */
    if (c == '"') {
        next_char();
        int i = 0;
        while (peek_char() != '"' && peek_char() != '\0' && peek_char() != '\n') {
            if (peek_char() == '\\') {
                next_char();
                char esc = next_char();
                switch (esc) {
                    case 'n': tok.text[i++] = '\n'; break;
                    case 'r': tok.text[i++] = '\r'; break;
                    case 't': tok.text[i++] = '\t'; break;
                    case '0': tok.text[i++] = '\0'; break;
                    case '\\': tok.text[i++] = '\\'; break;
                    case '"': tok.text[i++] = '"'; break;
                    default: tok.text[i++] = esc; break;
                }
            } else {
                tok.text[i++] = next_char();
            }
            if (i >= MAX_IDENTIFIER - 1) break;
        }
        tok.text[i] = '\0';
        if (peek_char() == '"') next_char();
        tok.type = TOK_STRING;
        return tok;
    }

    /* Character literal */
    if (c == '\'') {
        next_char();
        tok.value = 0;
        int i = 0;
        while (peek_char() != '\'' && peek_char() != '\0' && peek_char() != '\n') {
            char ch;
            if (peek_char() == '\\') {
                next_char();
                char esc = next_char();
                switch (esc) {
                    case 'n': ch = '\n'; break;
                    case 'r': ch = '\r'; break;
                    case 't': ch = '\t'; break;
                    case '0': ch = '\0'; break;
                    case '\\': ch = '\\'; break;
                    case '\'': ch = '\''; break;
                    default: ch = esc; break;
                }
            } else {
                ch = next_char();
            }
            tok.text[i++] = ch;
            /* Build value from characters (up to 4 bytes) */
            tok.value = (tok.value << 8) | (unsigned char)ch;
        }
        tok.text[i] = '\0';
        if (peek_char() == '\'') next_char();
        tok.type = TOK_CHAR;
        return tok;
    }

    /* Numbers */
    if (c == '$') {
        next_char();
        if (isxdigit(peek_char())) {
            /* Hex number with $ prefix */
            tok.value = parse_hex();
            tok.type = TOK_NUMBER;
            snprintf(tok.text, sizeof(tok.text), "$%lX", (unsigned long)tok.value);
        } else {
            /* $ alone means current address */
            tok.type = TOK_DOLLAR;
        }
        return tok;
    }

    if (c == '%') {
        next_char();
        tok.value = parse_binary();
        tok.type = TOK_NUMBER;
        snprintf(tok.text, sizeof(tok.text), "%%%lb", (unsigned long)tok.value);
        return tok;
    }

    if (c == '0' && (input_pos[1] == 'x' || input_pos[1] == 'X')) {
        next_char(); /* 0 */
        next_char(); /* x */
        tok.value = parse_hex();
        tok.type = TOK_NUMBER;
        snprintf(tok.text, sizeof(tok.text), "0x%lX", (unsigned long)tok.value);
        return tok;
    }

    if (isdigit(c)) {
        /* Could be decimal, hex with H suffix, or binary with B suffix */
        const char *start = input_pos;
        int64_t value = 0;

        /* Collect all hex digits */
        while (isxdigit(peek_char())) {
            value = value * 16 + (isdigit(peek_char()) ? peek_char() - '0' :
                    (toupper(peek_char()) - 'A' + 10));
            next_char();
        }

        /* Check suffix */
        if (peek_char() == 'H' || peek_char() == 'h') {
            next_char();
            tok.value = value;
            tok.type = TOK_NUMBER;
            snprintf(tok.text, sizeof(tok.text), "%lXH", (unsigned long)value);
            return tok;
        }

        if (peek_char() == 'B' || peek_char() == 'b') {
            /* Binary - re-parse */
            next_char();
            input_pos = start;
            value = 0;
            while (peek_char() == '0' || peek_char() == '1') {
                value = (value << 1) | (next_char() - '0');
            }
            if (peek_char() == 'B' || peek_char() == 'b') next_char();
            tok.value = value;
            tok.type = TOK_NUMBER;
            snprintf(tok.text, sizeof(tok.text), "%lbB", (unsigned long)value);
            return tok;
        }

        /* Decimal - re-parse from start */
        input_pos = start;
        tok.value = parse_decimal();
        tok.type = TOK_NUMBER;
        snprintf(tok.text, sizeof(tok.text), "%ld", (long)tok.value);
        return tok;
    }

    /* Identifiers */
    if (is_ident_start(c)) {
        int i = 0;
        while (is_ident_char(peek_char()) && i < MAX_IDENTIFIER - 1) {
            tok.text[i++] = next_char();
        }
        tok.text[i] = '\0';
        tok.type = TOK_IDENTIFIER;
        return tok;
    }

    /* Operators and punctuation */
    next_char();
    tok.text[0] = c;
    tok.text[1] = '\0';

    switch (c) {
        case ':': tok.type = TOK_COLON; break;
        case ',': tok.type = TOK_COMMA; break;
        case '(': tok.type = TOK_LPAREN; break;
        case ')': tok.type = TOK_RPAREN; break;
        case '+': tok.type = TOK_PLUS; break;
        case '-': tok.type = TOK_MINUS; break;
        case '*': tok.type = TOK_STAR; break;
        case '/': tok.type = TOK_SLASH; break;
        case '&':
            if (peek_char() == '&') {
                next_char();
                tok.text[1] = '&';
                tok.text[2] = '\0';
            }
            tok.type = TOK_AMPERSAND;
            break;
        case '|':
            if (peek_char() == '|') {
                next_char();
                tok.text[1] = '|';
                tok.text[2] = '\0';
            }
            tok.type = TOK_PIPE;
            break;
        case '^': tok.type = TOK_CARET; break;
        case '~': tok.type = TOK_TILDE; break;
        case '#': tok.type = TOK_HASH; break;
        case '.': tok.type = TOK_DOT; break;
        case '=':
            if (peek_char() == '=') {
                next_char();
                tok.text[1] = '=';
                tok.text[2] = '\0';
            }
            tok.type = TOK_EQUALS;
            break;
        case '<':
            if (peek_char() == '<') {
                next_char();
                tok.type = TOK_LSHIFT;
                tok.text[1] = '<';
                tok.text[2] = '\0';
            } else if (peek_char() == '=') {
                next_char();
                tok.text[1] = '=';
                tok.text[2] = '\0';
                tok.type = TOK_LT;
            } else {
                tok.type = TOK_LT;
            }
            break;
        case '>':
            if (peek_char() == '>') {
                next_char();
                tok.type = TOK_RSHIFT;
                tok.text[1] = '>';
                tok.text[2] = '\0';
            } else if (peek_char() == '=') {
                next_char();
                tok.text[1] = '=';
                tok.text[2] = '\0';
                tok.type = TOK_GT;
            } else {
                tok.type = TOK_GT;
            }
            break;
        case '!':
            if (peek_char() == '=') {
                next_char();
                tok.text[1] = '=';
                tok.text[2] = '\0';
            }
            tok.type = TOK_EXCLAIM;
            break;
        case '?': tok.type = TOK_QUESTION; break;
        case '\\': tok.type = TOK_BACKSLASH; break;
        case '@': tok.type = TOK_AT; break;
        default:
            /* Unknown character - return as-is, let parser handle */
            tok.type = TOK_EOF;
            break;
    }

    return tok;
}

Token lexer_peek(void) {
    if (!has_peeked) {
        peeked_token = lexer_next();
        has_peeked = true;
    }
    return peeked_token;
}
