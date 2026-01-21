/*
 * TLCS-900 Assembler - Expression Evaluator
 *
 * Recursive descent parser for arithmetic expressions.
 * Supports:
 * - Arithmetic: +, -, *, /, %
 * - Bitwise: &, |, ^, ~, <<, >>
 * - Comparison: <, >, <=, >=, ==, !=
 * - Logical: !, &&, ||
 * - Special: $ (current address)
 * - Symbols and numeric literals
 */

#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include "../include/tlcs900.h"

/* Forward declarations for recursive descent */
static bool parse_expr_or(Assembler *as, int64_t *result, bool *known, bool *is_const);
static bool parse_expr_and(Assembler *as, int64_t *result, bool *known, bool *is_const);
static bool parse_expr_bitor(Assembler *as, int64_t *result, bool *known, bool *is_const);
static bool parse_expr_bitxor(Assembler *as, int64_t *result, bool *known, bool *is_const);
static bool parse_expr_bitand(Assembler *as, int64_t *result, bool *known, bool *is_const);
static bool parse_expr_equality(Assembler *as, int64_t *result, bool *known, bool *is_const);
static bool parse_expr_relational(Assembler *as, int64_t *result, bool *known, bool *is_const);
static bool parse_expr_shift(Assembler *as, int64_t *result, bool *known, bool *is_const);
static bool parse_expr_additive(Assembler *as, int64_t *result, bool *known, bool *is_const);
static bool parse_expr_multiplicative(Assembler *as, int64_t *result, bool *known, bool *is_const);
static bool parse_expr_unary(Assembler *as, int64_t *result, bool *known, bool *is_const);
static bool parse_expr_primary(Assembler *as, int64_t *result, bool *known, bool *is_const);

/* External symbol lookup - returns symbol type too */
extern bool symbol_get_value(Assembler *as, const char *name, int64_t *value);
extern SymbolType symbol_get_type(Assembler *as, const char *name);

/* Main entry point */
bool expr_parse(Assembler *as, int64_t *result, bool *known, bool *is_constant) {
    *known = true;
    *is_constant = true;
    return parse_expr_or(as, result, known, is_constant);
}

/* Logical OR: || */
static bool parse_expr_or(Assembler *as, int64_t *result, bool *known, bool *is_const) {
    if (!parse_expr_and(as, result, known, is_const)) return false;

    while (lexer_peek().type == TOK_PIPE) {
        Token tok = lexer_peek();
        if (tok.text[1] == '|') {
            lexer_next(); /* consume || */
            int64_t right;
            bool right_known = true;
            bool right_const = true;
            if (!parse_expr_and(as, &right, &right_known, &right_const)) return false;
            *result = *result || right;
            *known = *known && right_known;
            *is_const = *is_const && right_const;
        } else {
            break;
        }
    }
    return true;
}

/* Logical AND: && */
static bool parse_expr_and(Assembler *as, int64_t *result, bool *known, bool *is_const) {
    if (!parse_expr_bitor(as, result, known, is_const)) return false;

    while (lexer_peek().type == TOK_AMPERSAND) {
        Token tok = lexer_peek();
        if (tok.text[1] == '&') {
            lexer_next(); /* consume && */
            int64_t right;
            bool right_known = true;
            bool right_const = true;
            if (!parse_expr_bitor(as, &right, &right_known, &right_const)) return false;
            *result = *result && right;
            *known = *known && right_known;
            *is_const = *is_const && right_const;
        } else {
            break;
        }
    }
    return true;
}

/* Bitwise OR: | */
static bool parse_expr_bitor(Assembler *as, int64_t *result, bool *known, bool *is_const) {
    if (!parse_expr_bitxor(as, result, known, is_const)) return false;

    while (lexer_peek().type == TOK_PIPE && lexer_peek().text[1] != '|') {
        lexer_next(); /* consume | */
        int64_t right;
        bool right_known = true;
        bool right_const = true;
        if (!parse_expr_bitxor(as, &right, &right_known, &right_const)) return false;
        *result = *result | right;
        *known = *known && right_known;
        *is_const = *is_const && right_const;
    }
    return true;
}

/* Bitwise XOR: ^ */
static bool parse_expr_bitxor(Assembler *as, int64_t *result, bool *known, bool *is_const) {
    if (!parse_expr_bitand(as, result, known, is_const)) return false;

    while (lexer_peek().type == TOK_CARET) {
        lexer_next(); /* consume ^ */
        int64_t right;
        bool right_known = true;
        bool right_const = true;
        if (!parse_expr_bitand(as, &right, &right_known, &right_const)) return false;
        *result = *result ^ right;
        *known = *known && right_known;
        *is_const = *is_const && right_const;
    }
    return true;
}

/* Bitwise AND: & */
static bool parse_expr_bitand(Assembler *as, int64_t *result, bool *known, bool *is_const) {
    if (!parse_expr_equality(as, result, known, is_const)) return false;

    while (lexer_peek().type == TOK_AMPERSAND && lexer_peek().text[1] != '&') {
        lexer_next(); /* consume & */
        int64_t right;
        bool right_known = true;
        bool right_const = true;
        if (!parse_expr_equality(as, &right, &right_known, &right_const)) return false;
        *result = *result & right;
        *known = *known && right_known;
        *is_const = *is_const && right_const;
    }
    return true;
}

/* Equality: ==, != */
static bool parse_expr_equality(Assembler *as, int64_t *result, bool *known, bool *is_const) {
    if (!parse_expr_relational(as, result, known, is_const)) return false;

    while (true) {
        Token tok = lexer_peek();
        if (tok.type == TOK_EQUALS && tok.text[1] == '=') {
            lexer_next(); /* consume == */
            int64_t right;
            bool right_known = true;
            bool right_const = true;
            if (!parse_expr_relational(as, &right, &right_known, &right_const)) return false;
            *result = (*result == right) ? 1 : 0;
            *known = *known && right_known;
            *is_const = *is_const && right_const;
        } else if (tok.type == TOK_EXCLAIM && tok.text[1] == '=') {
            lexer_next(); /* consume != */
            int64_t right;
            bool right_known = true;
            bool right_const = true;
            if (!parse_expr_relational(as, &right, &right_known, &right_const)) return false;
            *result = (*result != right) ? 1 : 0;
            *known = *known && right_known;
            *is_const = *is_const && right_const;
        } else {
            break;
        }
    }
    return true;
}

/* Relational: <, >, <=, >= */
static bool parse_expr_relational(Assembler *as, int64_t *result, bool *known, bool *is_const) {
    if (!parse_expr_shift(as, result, known, is_const)) return false;

    while (true) {
        Token tok = lexer_peek();
        if (tok.type == TOK_LT) {
            lexer_next();
            int64_t right;
            bool right_known = true;
            bool right_const = true;
            if (!parse_expr_shift(as, &right, &right_known, &right_const)) return false;
            if (tok.text[1] == '=') {
                *result = (*result <= right) ? 1 : 0;
            } else {
                *result = (*result < right) ? 1 : 0;
            }
            *known = *known && right_known;
            *is_const = *is_const && right_const;
        } else if (tok.type == TOK_GT) {
            lexer_next();
            int64_t right;
            bool right_known = true;
            bool right_const = true;
            if (!parse_expr_shift(as, &right, &right_known, &right_const)) return false;
            if (tok.text[1] == '=') {
                *result = (*result >= right) ? 1 : 0;
            } else {
                *result = (*result > right) ? 1 : 0;
            }
            *known = *known && right_known;
            *is_const = *is_const && right_const;
        } else {
            break;
        }
    }
    return true;
}

/* Shift: <<, >> */
static bool parse_expr_shift(Assembler *as, int64_t *result, bool *known, bool *is_const) {
    if (!parse_expr_additive(as, result, known, is_const)) return false;

    while (true) {
        Token tok = lexer_peek();
        if (tok.type == TOK_LSHIFT) {
            lexer_next(); /* consume << */
            int64_t right;
            bool right_known = true;
            bool right_const = true;
            if (!parse_expr_additive(as, &right, &right_known, &right_const)) return false;
            *result = *result << right;
            *known = *known && right_known;
            *is_const = *is_const && right_const;
        } else if (tok.type == TOK_RSHIFT) {
            lexer_next(); /* consume >> */
            int64_t right;
            bool right_known = true;
            bool right_const = true;
            if (!parse_expr_additive(as, &right, &right_known, &right_const)) return false;
            *result = *result >> right;
            *known = *known && right_known;
            *is_const = *is_const && right_const;
        } else {
            break;
        }
    }
    return true;
}

/* Additive: +, - */
static bool parse_expr_additive(Assembler *as, int64_t *result, bool *known, bool *is_const) {
    if (!parse_expr_multiplicative(as, result, known, is_const)) return false;

    while (true) {
        Token tok = lexer_peek();
        if (tok.type == TOK_PLUS) {
            lexer_next(); /* consume + */
            int64_t right;
            bool right_known = true;
            bool right_const = true;
            if (!parse_expr_multiplicative(as, &right, &right_known, &right_const)) return false;
            *result = *result + right;
            *known = *known && right_known;
            *is_const = *is_const && right_const;
        } else if (tok.type == TOK_MINUS) {
            lexer_next(); /* consume - */
            int64_t right;
            bool right_known = true;
            bool right_const = true;
            if (!parse_expr_multiplicative(as, &right, &right_known, &right_const)) return false;
            *result = *result - right;
            *known = *known && right_known;
            *is_const = *is_const && right_const;
        } else {
            break;
        }
    }
    return true;
}

/* Multiplicative: *, /, % */
static bool parse_expr_multiplicative(Assembler *as, int64_t *result, bool *known, bool *is_const) {
    if (!parse_expr_unary(as, result, known, is_const)) return false;

    while (true) {
        Token tok = lexer_peek();
        if (tok.type == TOK_STAR) {
            lexer_next(); /* consume * */
            int64_t right;
            bool right_known = true;
            bool right_const = true;
            if (!parse_expr_unary(as, &right, &right_known, &right_const)) return false;
            *result = *result * right;
            *known = *known && right_known;
            *is_const = *is_const && right_const;
        } else if (tok.type == TOK_SLASH) {
            lexer_next(); /* consume / */
            int64_t right;
            bool right_known = true;
            bool right_const = true;
            if (!parse_expr_unary(as, &right, &right_known, &right_const)) return false;
            if (right == 0) {
                error(as, "division by zero");
                return false;
            }
            *result = *result / right;
            *known = *known && right_known;
            *is_const = *is_const && right_const;
        } else if (tok.type == TOK_PERCENT) {
            lexer_next(); /* consume % (but not if it's binary prefix) */
            int64_t right;
            bool right_known = true;
            bool right_const = true;
            if (!parse_expr_unary(as, &right, &right_known, &right_const)) return false;
            if (right == 0) {
                error(as, "modulo by zero");
                return false;
            }
            *result = *result % right;
            *known = *known && right_known;
            *is_const = *is_const && right_const;
        } else {
            break;
        }
    }
    return true;
}

/* Unary: -, ~, !, + */
static bool parse_expr_unary(Assembler *as, int64_t *result, bool *known, bool *is_const) {
    Token tok = lexer_peek();

    if (tok.type == TOK_MINUS) {
        lexer_next(); /* consume - */
        if (!parse_expr_unary(as, result, known, is_const)) return false;
        *result = -*result;
        return true;
    }

    if (tok.type == TOK_PLUS) {
        lexer_next(); /* consume + */
        return parse_expr_unary(as, result, known, is_const);
    }

    if (tok.type == TOK_TILDE) {
        lexer_next(); /* consume ~ */
        if (!parse_expr_unary(as, result, known, is_const)) return false;
        *result = ~*result;
        return true;
    }

    if (tok.type == TOK_EXCLAIM && tok.text[1] != '=') {
        lexer_next(); /* consume ! */
        if (!parse_expr_unary(as, result, known, is_const)) return false;
        *result = !*result;
        return true;
    }

    return parse_expr_primary(as, result, known, is_const);
}

/* Primary: numbers, symbols, $, parenthesized expressions */
static bool parse_expr_primary(Assembler *as, int64_t *result, bool *known, bool *is_const) {
    Token tok = lexer_peek();

    /* Number literal - always constant */
    if (tok.type == TOK_NUMBER) {
        lexer_next();
        *result = tok.value;
        /* is_const stays true */
        return true;
    }

    /* Character literal - always constant */
    if (tok.type == TOK_CHAR) {
        lexer_next();
        *result = tok.value;
        /* is_const stays true */
        return true;
    }

    /* $ - current address - NOT constant (it's a label-like value) */
    if (tok.type == TOK_DOLLAR) {
        lexer_next();
        *result = as->pc;
        *is_const = false;  /* Address, not a numeric constant */
        return true;
    }

    /* Parenthesized expression */
    if (tok.type == TOK_LPAREN) {
        lexer_next(); /* consume ( */
        if (!parse_expr_or(as, result, known, is_const)) return false;
        tok = lexer_peek();
        if (tok.type != TOK_RPAREN) {
            error(as, "expected ')' in expression");
            return false;
        }
        lexer_next(); /* consume ) */
        return true;
    }

    /* Symbol reference */
    if (tok.type == TOK_IDENTIFIER) {
        lexer_next();

        /* Check for built-in functions */
        if (strcasecmp(tok.text, "HIGH") == 0 || strcasecmp(tok.text, "HI") == 0) {
            if (lexer_peek().type != TOK_LPAREN) {
                error(as, "expected '(' after HIGH");
                return false;
            }
            lexer_next(); /* consume ( */
            if (!parse_expr_or(as, result, known, is_const)) return false;
            if (lexer_peek().type != TOK_RPAREN) {
                error(as, "expected ')' after HIGH expression");
                return false;
            }
            lexer_next(); /* consume ) */
            *result = (*result >> 8) & 0xFF;
            return true;
        }

        if (strcasecmp(tok.text, "LOW") == 0 || strcasecmp(tok.text, "LO") == 0) {
            if (lexer_peek().type != TOK_LPAREN) {
                error(as, "expected '(' after LOW");
                return false;
            }
            lexer_next(); /* consume ( */
            if (!parse_expr_or(as, result, known, is_const)) return false;
            if (lexer_peek().type != TOK_RPAREN) {
                error(as, "expected ')' after LOW expression");
                return false;
            }
            lexer_next(); /* consume ) */
            *result = *result & 0xFF;
            return true;
        }

        if (strcasecmp(tok.text, "BANK") == 0) {
            if (lexer_peek().type != TOK_LPAREN) {
                error(as, "expected '(' after BANK");
                return false;
            }
            lexer_next(); /* consume ( */
            if (!parse_expr_or(as, result, known, is_const)) return false;
            if (lexer_peek().type != TOK_RPAREN) {
                error(as, "expected ')' after BANK expression");
                return false;
            }
            lexer_next(); /* consume ) */
            *result = (*result >> 16) & 0xFF;
            return true;
        }

        /* Regular symbol lookup */
        if (symbol_get_value(as, tok.text, result)) {
            /* Check symbol type to determine if it's a constant */
            SymbolType sym_type = symbol_get_type(as, tok.text);
            if (sym_type == SYM_EQU || sym_type == SYM_SET) {
                /* EQU/SET symbols are constants */
                /* is_const stays true */
            } else {
                /* Labels are not constants - they're addresses */
                *is_const = false;
            }
            return true;
        }

        /* Symbol not defined yet - might be forward reference */
        if (as->pass == 1) {
            *result = 0;
            *known = false;
            *is_const = false;  /* Unknown symbol, assume it's a label */
            return true;
        }

        error(as, "undefined symbol '%s'", tok.text);
        return false;
    }

    error(as, "expected expression, got '%s'", tok.text);
    return false;
}
