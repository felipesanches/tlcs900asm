/*
 * TLCS-900/TMP94C241 Assembler
 * CPU definitions, opcodes, and register encodings
 */

#ifndef TLCS900_H
#define TLCS900_H

#include <stdint.h>
#include <stdbool.h>

/* Maximum limits */
#define MAX_LINE_LENGTH     4096
#define MAX_IDENTIFIER      256
#define MAX_OPERANDS        4
#define MAX_INCLUDE_DEPTH   16
#define MAX_MACRO_PARAMS    16
#define MAX_MACRO_DEPTH     16

/* Token types */
typedef enum {
    TOK_EOF = 0,
    TOK_NEWLINE,
    TOK_IDENTIFIER,
    TOK_NUMBER,
    TOK_STRING,
    TOK_CHAR,
    TOK_COLON,
    TOK_COMMA,
    TOK_LPAREN,
    TOK_RPAREN,
    TOK_PLUS,
    TOK_MINUS,
    TOK_STAR,
    TOK_SLASH,
    TOK_PERCENT,
    TOK_AMPERSAND,
    TOK_PIPE,
    TOK_CARET,
    TOK_TILDE,
    TOK_LSHIFT,
    TOK_RSHIFT,
    TOK_DOLLAR,      /* $ - current address */
    TOK_HASH,        /* # - immediate prefix (optional in some syntaxes) */
    TOK_DOT,
    TOK_EQUALS,
    TOK_LT,
    TOK_GT,
    TOK_EXCLAIM,
    TOK_QUESTION,
    TOK_BACKSLASH,
    TOK_AT,
} TokenType;

/* Token structure */
typedef struct {
    TokenType type;
    char text[MAX_IDENTIFIER];
    int64_t value;          /* For numbers */
    int line;
    int column;
} Token;

/* Lexer state for save/restore */
typedef struct {
    const char *pos;
    int line;
    int column;
    Token peeked;
    bool has_peeked;
} LexerState;

/* Register types */
typedef enum {
    REG_NONE = 0,
    /* 8-bit registers */
    REG_A, REG_W, REG_C, REG_B, REG_E, REG_D, REG_L, REG_H,
    REG_QA, REG_QW, REG_QC, REG_QB, REG_QE, REG_QD, REG_QL, REG_QH,
    REG_IXL, REG_IXH, REG_IYL, REG_IYH, REG_IZL, REG_IZH,
    REG_QIXL, REG_QIXH, REG_QIYL, REG_QIYH, REG_QIZL, REG_QIZH,
    /* 16-bit registers */
    REG_WA, REG_BC, REG_DE, REG_HL, REG_IX, REG_IY, REG_IZ, REG_SP,
    REG_QWA, REG_QBC, REG_QDE, REG_QHL, REG_QIX, REG_QIY, REG_QIZ,
    /* 32-bit registers */
    REG_XWA, REG_XBC, REG_XDE, REG_XHL, REG_XIX, REG_XIY, REG_XIZ, REG_XSP,
    REG_QXWA, REG_QXBC, REG_QXDE, REG_QXHL,
    /* Special registers */
    REG_PC, REG_SR, REG_F, REG_F_PRIME,
    /* Previous bank registers (for PUSH/POP) */
    REG_A_PREV, REG_W_PREV, REG_BC_PREV, REG_DE_PREV, REG_HL_PREV,
} RegisterType;

/* Operand size */
typedef enum {
    SIZE_NONE = 0,
    SIZE_BYTE = 1,      /* 8-bit */
    SIZE_WORD = 2,      /* 16-bit */
    SIZE_LONG = 4,      /* 32-bit */
} OperandSize;

/* Addressing modes */
typedef enum {
    ADDR_NONE = 0,
    ADDR_IMMEDIATE,         /* #nn or just nn */
    ADDR_REGISTER,          /* r, rr, xrr */
    ADDR_REGISTER_IND,      /* (rr), (xrr) */
    ADDR_REGISTER_IND_INC,  /* (xrr+) */
    ADDR_REGISTER_IND_DEC,  /* (-xrr) */
    ADDR_INDEXED,           /* (xrr + d8/d16) */
    ADDR_INDEXED_REG,       /* (xrr + r8) */
    ADDR_DIRECT,            /* (nn) - memory direct */
    ADDR_RELATIVE,          /* PC-relative for jumps */
    ADDR_BIT,               /* bit number */
    ADDR_CONDITION,         /* condition code */
} AddressingMode;

/* Condition codes */
typedef enum {
    CC_F = 0,   /* False/never */
    CC_LT,      /* Less than (signed) */
    CC_LE,      /* Less or equal (signed) */
    CC_ULE,     /* Unsigned less or equal */
    CC_PE,      /* Parity even / Overflow */
    CC_MI,      /* Minus / Negative */
    CC_Z,       /* Zero */
    CC_C,       /* Carry */
    CC_T,       /* True/always */
    CC_GE,      /* Greater or equal (signed) */
    CC_GT,      /* Greater than (signed) */
    CC_UGT,     /* Unsigned greater than */
    CC_PO,      /* Parity odd / No overflow */
    CC_PL,      /* Plus / Positive */
    CC_NZ,      /* Not zero */
    CC_NC,      /* No carry */
    /* Aliases */
    CC_EQ = CC_Z,
    CC_NE = CC_NZ,
    CC_OV = CC_PE,
    CC_NOV = CC_PO,
    CC_M = CC_MI,
    CC_P = CC_PL,
    CC_ULT = CC_C,
    CC_UGE = CC_NC,
} ConditionCode;

/* Operand structure */
typedef struct {
    AddressingMode mode;
    OperandSize size;
    RegisterType reg;
    RegisterType index_reg;     /* For indexed addressing */
    int64_t value;              /* Immediate/displacement value */
    bool value_known;           /* Is value resolved? */
    char symbol[MAX_IDENTIFIER]; /* Unresolved symbol name */
    int addr_size;              /* :8, :16, :24 suffix */
} Operand;

/* Instruction table entry */
typedef struct {
    const char *mnemonic;
    uint8_t base_opcode;
    int operand_count;
    /* Encoding function pointer will be added */
} InstructionDef;

/* Symbol types */
typedef enum {
    SYM_LABEL,
    SYM_EQU,
    SYM_SET,        /* Reassignable */
    SYM_MACRO,
    SYM_SECTION,
} SymbolType;

/* Symbol entry */
typedef struct Symbol {
    char name[MAX_IDENTIFIER];
    SymbolType type;
    int64_t value;
    bool defined;
    bool referenced;
    int definition_line;
    const char *definition_file;
    struct Symbol *next;    /* For hash chain */
    /* For macros */
    char **macro_body;
    int macro_body_lines;
    char **macro_params;
    int macro_param_count;
} Symbol;

/* Assembler state */
typedef struct {
    /* Current position */
    uint32_t pc;                /* Program counter */
    uint32_t org;               /* Current origin */

    /* Output buffer */
    uint8_t *output;
    size_t output_size;
    size_t output_capacity;
    uint32_t output_base;       /* Base address for output */

    /* Symbol table */
    Symbol **symbols;
    size_t symbol_table_size;

    /* Current file context */
    const char *current_file;
    int current_line;

    /* Include stack */
    struct {
        const char *filename;
        FILE *fp;
        int line;
    } include_stack[MAX_INCLUDE_DEPTH];
    int include_depth;

    /* Macro expansion */
    int macro_depth;

    /* Pass tracking */
    int pass;                   /* 1 or 2 */
    bool errors;
    int error_count;
    int warning_count;

    /* Options */
    bool max_mode;              /* MAXMODE directive */
    bool verbose;
    bool list_enabled;
} Assembler;

/* Function prototypes - will be expanded */

/* Lexer */
void lexer_init(const char *input);
Token lexer_next(void);
Token lexer_peek(void);
void lexer_push_back(Token tok);
void lexer_set_line(int line);
void lexer_save_state(LexerState *state);
void lexer_restore_state(const LexerState *state);

/* Symbols */
void symbols_init(Assembler *as);
void symbols_free(Assembler *as);
Symbol *symbol_lookup(Assembler *as, const char *name);
Symbol *symbol_define(Assembler *as, const char *name, SymbolType type, int64_t value);
bool symbol_is_defined(Assembler *as, const char *name);

/* Expressions */
bool expr_parse(Assembler *as, int64_t *result, bool *known);

/* Parser */
bool parse_line(Assembler *as, const char *line);
bool parse_operand(Assembler *as, Operand *op);

/* Code generation */
void emit_byte(Assembler *as, uint8_t b);
void emit_word(Assembler *as, uint16_t w);
void emit_long(Assembler *as, uint32_t l);
bool encode_instruction(Assembler *as, const char *mnemonic, Operand *operands, int operand_count);

/* Directives */
bool handle_directive(Assembler *as, const char *directive, const char *args);

/* Main assembler */
Assembler *assembler_new(void);
void assembler_free(Assembler *as);
bool assembler_assemble_file(Assembler *as, const char *filename);
bool assembler_write_output(Assembler *as, const char *filename);

/* Error reporting */
void error(Assembler *as, const char *fmt, ...);
void warning(Assembler *as, const char *fmt, ...);

#endif /* TLCS900_H */
