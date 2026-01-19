/*
 * TLCS-900 Assembler - Line Parser
 *
 * Parses assembly source lines, distinguishing between:
 * - Labels (identifier followed by colon)
 * - Directives (ORG, EQU, DB, etc.)
 * - Instructions (CPU mnemonics)
 * - Comments (starting with ;)
 */

#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include "../include/tlcs900.h"

/* External functions */
extern bool handle_directive(Assembler *as, const char *directive, const char *label);
extern bool encode_instruction(Assembler *as, const char *mnemonic, Operand *operands, int operand_count);

/* Macro functions */
extern bool macro_is_collecting(void);
extern bool macro_add_line(const char *line);
extern bool macro_try_expand(Assembler *as, const char *name, const char *args_str);

/* Forward declarations */
static bool is_register(const char *name, RegisterType *reg, OperandSize *size);
static bool is_condition(const char *name, ConditionCode *cc);
static bool parse_operand_internal(Assembler *as, Operand *op);

/* Register name table */
static const struct {
    const char *name;
    RegisterType reg;
    OperandSize size;
} register_table[] = {
    /* 8-bit registers */
    {"A", REG_A, SIZE_BYTE}, {"W", REG_W, SIZE_BYTE},
    {"C", REG_C, SIZE_BYTE}, {"B", REG_B, SIZE_BYTE},
    {"E", REG_E, SIZE_BYTE}, {"D", REG_D, SIZE_BYTE},
    {"L", REG_L, SIZE_BYTE}, {"H", REG_H, SIZE_BYTE},
    {"QA", REG_QA, SIZE_BYTE}, {"QW", REG_QW, SIZE_BYTE},
    {"QC", REG_QC, SIZE_BYTE}, {"QB", REG_QB, SIZE_BYTE},
    {"QE", REG_QE, SIZE_BYTE}, {"QD", REG_QD, SIZE_BYTE},
    {"QL", REG_QL, SIZE_BYTE}, {"QH", REG_QH, SIZE_BYTE},
    /* 16-bit registers */
    {"WA", REG_WA, SIZE_WORD}, {"BC", REG_BC, SIZE_WORD},
    {"DE", REG_DE, SIZE_WORD}, {"HL", REG_HL, SIZE_WORD},
    {"IX", REG_IX, SIZE_WORD}, {"IY", REG_IY, SIZE_WORD},
    {"IZ", REG_IZ, SIZE_WORD}, {"SP", REG_SP, SIZE_WORD},
    {"QWA", REG_QWA, SIZE_WORD}, {"QBC", REG_QBC, SIZE_WORD},
    {"QDE", REG_QDE, SIZE_WORD}, {"QHL", REG_QHL, SIZE_WORD},
    /* 32-bit registers */
    {"XWA", REG_XWA, SIZE_LONG}, {"XBC", REG_XBC, SIZE_LONG},
    {"XDE", REG_XDE, SIZE_LONG}, {"XHL", REG_XHL, SIZE_LONG},
    {"XIX", REG_XIX, SIZE_LONG}, {"XIY", REG_XIY, SIZE_LONG},
    {"XIZ", REG_XIZ, SIZE_LONG}, {"XSP", REG_XSP, SIZE_LONG},
    {"QXWA", REG_QXWA, SIZE_LONG}, {"QXBC", REG_QXBC, SIZE_LONG},
    {"QXDE", REG_QXDE, SIZE_LONG}, {"QXHL", REG_QXHL, SIZE_LONG},
    /* Special */
    {"PC", REG_PC, SIZE_LONG},
    {"SR", REG_SR, SIZE_WORD},
    {"F", REG_F, SIZE_BYTE},
    {"F'", REG_F_PRIME, SIZE_BYTE},
    {NULL, REG_NONE, SIZE_NONE}
};

/* Condition code table */
static const struct {
    const char *name;
    ConditionCode cc;
} condition_table[] = {
    {"F", CC_F}, {"LT", CC_LT}, {"LE", CC_LE}, {"ULE", CC_ULE},
    {"PE", CC_PE}, {"OV", CC_OV}, {"MI", CC_MI}, {"M", CC_M},
    {"Z", CC_Z}, {"EQ", CC_EQ}, {"C", CC_C}, {"ULT", CC_ULT},
    {"T", CC_T}, {"GE", CC_GE}, {"GT", CC_GT}, {"UGT", CC_UGT},
    {"PO", CC_PO}, {"NOV", CC_NOV}, {"PL", CC_PL}, {"P", CC_P},
    {"NZ", CC_NZ}, {"NE", CC_NE}, {"NC", CC_NC}, {"UGE", CC_UGE},
    {NULL, CC_F}
};

/* Check if a name is a register */
static bool is_register(const char *name, RegisterType *reg, OperandSize *size) {
    for (int i = 0; register_table[i].name; i++) {
        if (strcasecmp(name, register_table[i].name) == 0) {
            if (reg) *reg = register_table[i].reg;
            if (size) *size = register_table[i].size;
            return true;
        }
    }
    return false;
}

/* Check if a name is a condition code */
static bool is_condition(const char *name, ConditionCode *cc) {
    for (int i = 0; condition_table[i].name; i++) {
        if (strcasecmp(name, condition_table[i].name) == 0) {
            if (cc) *cc = condition_table[i].cc;
            return true;
        }
    }
    return false;
}

/* Parse a single operand */
bool parse_operand(Assembler *as, Operand *op) {
    memset(op, 0, sizeof(*op));
    return parse_operand_internal(as, op);
}

static bool parse_operand_internal(Assembler *as, Operand *op) {
    Token tok = lexer_peek();

    /* Empty operand */
    if (tok.type == TOK_NEWLINE || tok.type == TOK_EOF || tok.type == TOK_COMMA) {
        return false;
    }

    /* Parenthesized addressing mode */
    if (tok.type == TOK_LPAREN) {
        lexer_next();  /* consume ( */

        tok = lexer_peek();

        /* Check for register-based addressing */
        if (tok.type == TOK_IDENTIFIER) {
            RegisterType reg;
            OperandSize size;

            if (is_register(tok.text, &reg, &size)) {
                lexer_next();  /* consume register */

                tok = lexer_peek();

                /* (reg+) - post-increment */
                if (tok.type == TOK_PLUS) {
                    lexer_next();
                    tok = lexer_peek();
                    if (tok.type == TOK_RPAREN) {
                        lexer_next();
                        op->mode = ADDR_REGISTER_IND_INC;
                        op->reg = reg;
                        op->size = size;
                        goto check_addr_size;
                    }
                    /* (reg + offset) - indexed */
                    int64_t offset;
                    bool known;
                    if (!expr_parse(as, &offset, &known)) {
                        error(as, "invalid indexed offset");
                        return false;
                    }
                    op->value = offset;
                    op->value_known = known;
                    tok = lexer_peek();
                    /* Check for :8/:16/:24 size suffix inside parentheses */
                    if (tok.type == TOK_COLON) {
                        lexer_next();
                        tok = lexer_peek();
                        if (tok.type == TOK_NUMBER) {
                            lexer_next();
                            op->addr_size = (int)tok.value;
                        }
                        tok = lexer_peek();
                    }
                    if (tok.type != TOK_RPAREN) {
                        error(as, "expected ')' after indexed addressing");
                        return false;
                    }
                    lexer_next();
                    op->mode = ADDR_INDEXED;
                    op->reg = reg;
                    op->size = size;
                    goto check_addr_size;
                }

                /* (reg - offset) - indexed with negative */
                if (tok.type == TOK_MINUS) {
                    lexer_next();
                    int64_t offset;
                    bool known;
                    if (!expr_parse(as, &offset, &known)) {
                        error(as, "invalid indexed offset");
                        return false;
                    }
                    op->value = -offset;
                    op->value_known = known;
                    tok = lexer_peek();
                    /* Check for :8/:16/:24 size suffix inside parentheses */
                    if (tok.type == TOK_COLON) {
                        lexer_next();
                        tok = lexer_peek();
                        if (tok.type == TOK_NUMBER) {
                            lexer_next();
                            op->addr_size = (int)tok.value;
                        }
                        tok = lexer_peek();
                    }
                    if (tok.type != TOK_RPAREN) {
                        error(as, "expected ')' after indexed addressing");
                        return false;
                    }
                    lexer_next();
                    op->mode = ADDR_INDEXED;
                    op->reg = reg;
                    op->size = size;
                    goto check_addr_size;
                }

                /* (reg) - simple indirect */
                if (tok.type == TOK_RPAREN) {
                    lexer_next();
                    op->mode = ADDR_REGISTER_IND;
                    op->reg = reg;
                    op->size = size;
                    goto check_addr_size;
                }

                error(as, "unexpected token in addressing mode");
                return false;
            }
        }

        /* (-reg) - pre-decrement */
        if (tok.type == TOK_MINUS) {
            lexer_next();
            tok = lexer_peek();
            if (tok.type == TOK_IDENTIFIER) {
                RegisterType reg;
                OperandSize size;
                if (is_register(tok.text, &reg, &size)) {
                    lexer_next();
                    tok = lexer_peek();
                    if (tok.type == TOK_RPAREN) {
                        lexer_next();
                        op->mode = ADDR_REGISTER_IND_DEC;
                        op->reg = reg;
                        op->size = size;
                        goto check_addr_size;
                    }
                }
            }
            error(as, "invalid pre-decrement addressing");
            return false;
        }

        /* (expression) - direct memory addressing */
        int64_t addr;
        bool known;
        if (!expr_parse(as, &addr, &known)) {
            error(as, "invalid address expression");
            return false;
        }
        op->value = addr;
        op->value_known = known;

        tok = lexer_peek();
        if (tok.type != TOK_RPAREN) {
            error(as, "expected ')' after address");
            return false;
        }
        lexer_next();
        op->mode = ADDR_DIRECT;
        goto check_addr_size;
    }

    /* Check for register */
    if (tok.type == TOK_IDENTIFIER) {
        RegisterType reg;
        OperandSize size;
        ConditionCode cc;

        bool is_reg = is_register(tok.text, &reg, &size);
        bool is_cc = is_condition(tok.text, &cc);

        /* If both register and condition code, look ahead to disambiguate */
        /* JR C, label - C is condition, second operand is immediate/label */
        /* LD C, (mem) - C is register, second operand starts with ( */
        if (is_reg && is_cc) {
            /* Save state before lookahead */
            LexerState saved;
            lexer_save_state(&saved);

            lexer_next();  /* consume the identifier (C, Z, etc.) */
            Token next = lexer_peek();
            if (next.type == TOK_COMMA) {
                /* Look ahead past the comma */
                lexer_next();  /* consume comma */
                Token after_comma = lexer_peek();

                /* Restore to just after the identifier */
                lexer_restore_state(&saved);
                lexer_next();  /* re-consume identifier */

                /* If next operand starts with (, #, $, number, or is a register, treat as register */
                if (after_comma.type == TOK_LPAREN ||
                    after_comma.type == TOK_HASH ||
                    after_comma.type == TOK_DOLLAR ||
                    after_comma.type == TOK_NUMBER ||
                    (after_comma.type == TOK_IDENTIFIER &&
                     is_register(after_comma.text, NULL, NULL))) {
                    op->mode = ADDR_REGISTER;
                    op->reg = reg;
                    op->size = size;
                    return true;
                }
                /* Otherwise treat as condition code */
                op->mode = ADDR_CONDITION;
                op->value = cc;
                return true;
            }
            /* Not followed by comma - treat as register */
            op->mode = ADDR_REGISTER;
            op->reg = reg;
            op->size = size;
            return true;
        }

        if (is_reg) {
            lexer_next();
            op->mode = ADDR_REGISTER;
            op->reg = reg;
            op->size = size;
            return true;
        }

        /* Check for condition code */
        if (is_cc) {
            lexer_next();
            op->mode = ADDR_CONDITION;
            op->value = cc;
            return true;
        }
    }

    /* Must be an immediate or symbol */
    /* Skip optional # prefix */
    if (tok.type == TOK_HASH) {
        lexer_next();
    }

    int64_t value;
    bool known;
    if (!expr_parse(as, &value, &known)) {
        /* Could be a symbol - save for later */
        if (tok.type == TOK_IDENTIFIER) {
            lexer_next();
            op->mode = ADDR_IMMEDIATE;
            strncpy(op->symbol, tok.text, MAX_IDENTIFIER - 1);
            op->value_known = false;
            return true;
        }
        error(as, "invalid operand");
        return false;
    }

    op->mode = ADDR_IMMEDIATE;
    op->value = value;
    op->value_known = known;
    return true;

check_addr_size:
    /* Check for :8, :16, :24 size suffix */
    tok = lexer_peek();
    if (tok.type == TOK_COLON) {
        lexer_next();
        tok = lexer_peek();
        if (tok.type == TOK_NUMBER) {
            lexer_next();
            op->addr_size = (int)tok.value;
        }
    }
    return true;
}

/* Parse a line of assembly */
bool parse_line(Assembler *as, const char *line) {
    /* Skip empty lines and comment-only lines */
    const char *p = line;
    while (*p == ' ' || *p == '\t') p++;
    if (*p == '\0' || *p == ';' || *p == '\n') {
        return true;
    }

    /* If collecting a macro definition, add lines to it */
    if (macro_is_collecting()) {
        /* Check for ENDM first */
        const char *check = p;
        while (*check == ' ' || *check == '\t') check++;
        if (strncasecmp(check, "ENDM", 4) == 0) {
            char c = check[4];
            if (c == '\0' || c == ' ' || c == '\t' || c == '\n' || c == ';') {
                /* End of macro - process through normal path */
                /* Fall through to normal parsing */
            } else {
                /* Part of macro body */
                macro_add_line(line);
                return true;
            }
        } else {
            /* Add to macro body */
            macro_add_line(line);
            return true;
        }
    }

    /* Initialize lexer with this line */
    lexer_init(line);
    lexer_set_line(as->current_line);

    Token tok = lexer_next();
    char label[MAX_IDENTIFIER] = "";
    char mnemonic[MAX_IDENTIFIER] = "";

    /* Check for label (identifier followed by colon, or identifier at column 1) */
    if (tok.type == TOK_IDENTIFIER) {
        Token next = lexer_peek();

        if (next.type == TOK_COLON) {
            /* Label with colon */
            strncpy(label, tok.text, MAX_IDENTIFIER - 1);
            lexer_next();  /* consume colon */
            tok = lexer_next();  /* get next token */
        } else if (line[0] != ' ' && line[0] != '\t') {
            /* Identifier at column 1 without colon */
            /* Check if next token is MACRO, EQU, SET, = (label-requiring directives) */
            if (next.type == TOK_IDENTIFIER &&
                (strcasecmp(next.text, "MACRO") == 0 ||
                 strcasecmp(next.text, "EQU") == 0 ||
                 strcasecmp(next.text, "SET") == 0)) {
                /* This is a label followed by a directive */
                strncpy(label, tok.text, MAX_IDENTIFIER - 1);
                tok = lexer_next();  /* get the directive */
                strncpy(mnemonic, tok.text, MAX_IDENTIFIER - 1);
            } else if (next.type == TOK_EQUALS) {
                /* label = value syntax */
                strncpy(label, tok.text, MAX_IDENTIFIER - 1);
                tok = lexer_next();  /* get the = */
            } else {
                /* Treat as mnemonic */
                strncpy(mnemonic, tok.text, MAX_IDENTIFIER - 1);
            }
        } else {
            strncpy(mnemonic, tok.text, MAX_IDENTIFIER - 1);
        }
    }

    /* If we have a label but no mnemonic yet, get the mnemonic */
    if (label[0] && !mnemonic[0]) {
        if (tok.type == TOK_IDENTIFIER) {
            strncpy(mnemonic, tok.text, MAX_IDENTIFIER - 1);
        } else if (tok.type == TOK_NEWLINE || tok.type == TOK_EOF) {
            /* Label only - define it */
            symbol_define(as, label, SYM_LABEL, as->pc);
            return true;
        }
    }

    /* Empty line after label */
    if (tok.type == TOK_NEWLINE || tok.type == TOK_EOF) {
        if (label[0]) {
            symbol_define(as, label, SYM_LABEL, as->pc);
        }
        return true;
    }

    /* Check for directive first (MACRO, EQU, SET handle their own symbol definition) */
    if (mnemonic[0] && handle_directive(as, mnemonic, label)) {
        return true;
    }

    /* Define label if present (and not handled by a directive) */
    if (label[0]) {
        symbol_define(as, label, SYM_LABEL, as->pc);
    }

    /* Check for = (alternate EQU syntax) */
    if (tok.type == TOK_EQUALS || (lexer_peek().type == TOK_EQUALS && label[0])) {
        if (tok.type == TOK_EQUALS) {
            lexer_next();
        } else if (lexer_peek().type == TOK_EQUALS) {
            lexer_next();
        }
        int64_t value;
        bool known;
        if (!expr_parse(as, &value, &known)) {
            error(as, "invalid expression after =");
            return false;
        }
        if (label[0]) {
            symbol_define(as, label, SYM_EQU, value);
        }
        return true;
    }

    /* Must be an instruction */
    if (!mnemonic[0]) {
        error(as, "expected instruction or directive");
        return false;
    }

    /* Parse operands */
    Operand operands[MAX_OPERANDS];
    int operand_count = 0;

    while (operand_count < MAX_OPERANDS) {
        Token peek = lexer_peek();
        if (peek.type == TOK_NEWLINE || peek.type == TOK_EOF) {
            break;
        }

        if (!parse_operand(as, &operands[operand_count])) {
            break;
        }
        operand_count++;

        peek = lexer_peek();
        if (peek.type == TOK_COMMA) {
            lexer_next();  /* consume comma */
        } else {
            break;
        }
    }

    /* Try to encode as an instruction first */
    if (encode_instruction(as, mnemonic, operands, operand_count)) {
        return true;
    }

    /* Not a known instruction - try macro expansion */
    /* Build the argument string from the rest of the line */
    char args_str[MAX_LINE_LENGTH] = "";
    int args_pos = 0;
    for (int i = 0; i < operand_count; i++) {
        if (i > 0 && args_pos < (int)sizeof(args_str) - 2) {
            args_str[args_pos++] = ',';
            args_str[args_pos++] = ' ';
        }
        /* Reconstruct operand as string (simplified) */
        char op_str[256] = "";
        if (operands[i].mode == ADDR_IMMEDIATE) {
            if (operands[i].value_known) {
                snprintf(op_str, sizeof(op_str), "%ld", (long)operands[i].value);
            } else if (operands[i].symbol[0]) {
                strncpy(op_str, operands[i].symbol, sizeof(op_str) - 1);
            }
        } else if (operands[i].mode == ADDR_REGISTER) {
            /* Get register name from code */
            for (int j = 0; register_table[j].name; j++) {
                if (register_table[j].reg == operands[i].reg) {
                    strncpy(op_str, register_table[j].name, sizeof(op_str) - 1);
                    break;
                }
            }
        }
        size_t len = strlen(op_str);
        if (args_pos + len < sizeof(args_str) - 1) {
            strcpy(args_str + args_pos, op_str);
            args_pos += len;
        }
    }
    args_str[args_pos] = '\0';

    if (macro_try_expand(as, mnemonic, args_str)) {
        return true;
    }

    /* Nothing worked - this is an unknown instruction/macro */
    error(as, "unknown instruction or macro: %s", mnemonic);
    return false;
}
