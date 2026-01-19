/*
 * TLCS-900 Assembler - Code Generator
 *
 * Instruction encoding for the TLCS-900/H CPU.
 *
 * Encoding patterns:
 * - 8-bit register prefix: 0xC8 + pair (C8=W/A, C9=B/C, CA=D/E, CB=H/L)
 * - 16-bit register prefix: 0xD8 + reg (D8=WA, D9=BC, DA=DE, DB=HL, DC=IX, DD=IY, DE=IZ)
 * - 32-bit register prefix: 0xE8 + reg (E8=XWA, E9=XBC, EA=XDE, EB=XHL, EC=XIX, ED=XIY, EE=XIZ)
 *
 * Operation codes (after prefix):
 * - 0x08=MUL, 0x09=MULS, 0x0A=DIV, 0x0B=DIVS
 * - 0xE8=RLC, 0xE9=RRC, 0xEA=RL, 0xEB=RR
 * - 0xEC=SLA, 0xED=SRA, 0xEE=SLL, 0xEF=SRL
 */

#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include "../include/tlcs900.h"

/* Get 8-bit register encoding */
static int get_reg8_code(RegisterType reg) {
    switch (reg) {
        /* Current bank */
        case REG_W: return 0; case REG_A: return 1;
        case REG_B: return 2; case REG_C: return 3;
        case REG_D: return 4; case REG_E: return 5;
        case REG_H: return 6; case REG_L: return 7;
        /* Index register low/high bytes */
        case REG_IXL: return 8; case REG_IXH: return 9;
        case REG_IYL: return 10; case REG_IYH: return 11;
        case REG_IZL: return 12; case REG_IZH: return 13;
        /* Q-bank */
        case REG_QW: return 16; case REG_QA: return 17;
        case REG_QB: return 18; case REG_QC: return 19;
        case REG_QD: return 20; case REG_QE: return 21;
        case REG_QH: return 22; case REG_QL: return 23;
        /* Q-bank index bytes */
        case REG_QIXL: return 24; case REG_QIXH: return 25;
        case REG_QIYL: return 26; case REG_QIYH: return 27;
        case REG_QIZL: return 28; case REG_QIZH: return 29;
        default: return -1;
    }
}

/* Get 16-bit register encoding */
static int get_reg16_code(RegisterType reg) {
    switch (reg) {
        /* Current bank */
        case REG_WA: return 0; case REG_BC: return 1;
        case REG_DE: return 2; case REG_HL: return 3;
        case REG_IX: return 4; case REG_IY: return 5;
        case REG_IZ: return 6; case REG_SP: return 7;
        /* Q-bank */
        case REG_QWA: return 8; case REG_QBC: return 9;
        case REG_QDE: return 10; case REG_QHL: return 11;
        case REG_QIX: return 12; case REG_QIY: return 13;
        case REG_QIZ: return 14;
        default: return -1;
    }
}

/* Get 32-bit register encoding */
static int get_reg32_code(RegisterType reg) {
    switch (reg) {
        case REG_XWA: return 0; case REG_XBC: return 1;
        case REG_XDE: return 2; case REG_XHL: return 3;
        case REG_XIX: return 4; case REG_XIY: return 5;
        case REG_XIZ: return 6; case REG_XSP: return 7;
        default: return -1;
    }
}

/* Get register pair prefix byte for 8-bit ops */
static int get_reg8_prefix(RegisterType reg) {
    int code = get_reg8_code(reg);
    if (code >= 0 && code < 8) {
        return 0xC8 + (code >> 1);  /* Pairs: W/A, B/C, D/E, H/L */
    } else if (code >= 8 && code < 14) {
        return 0xD0 + ((code - 8) >> 1);  /* Index pairs: IXL/IXH, IYL/IYH, IZL/IZH */
    } else if (code >= 16 && code < 24) {
        return 0xD8 + ((code - 16) >> 1);  /* Q-bank pairs */
    } else if (code >= 24 && code < 30) {
        return 0xE0 + ((code - 24) >> 1);  /* Q-bank index pairs */
    }
    return -1;
}

/* Get condition code encoding */
static int get_cc_code(int64_t cc) {
    switch ((ConditionCode)cc) {
        case CC_F:   return 0x0;
        case CC_LT:  return 0x1;
        case CC_LE:  return 0x2;
        case CC_ULE: return 0x3;
        case CC_PE:  return 0x4;
        case CC_MI:  return 0x5;
        case CC_Z:   return 0x6;
        case CC_C:   return 0x7;
        case CC_T:   return 0x8;
        case CC_GE:  return 0x9;
        case CC_GT:  return 0xA;
        case CC_UGT: return 0xB;
        case CC_PO:  return 0xC;
        case CC_PL:  return 0xD;
        case CC_NZ:  return 0xE;
        case CC_NC:  return 0xF;
        default: return 0x8;  /* Default to T (always) */
    }
}

/* Emit memory operand encoding */
static bool emit_mem_operand(Assembler *as, Operand *op) {
    switch (op->mode) {
        case ADDR_REGISTER_IND: {
            int code = get_reg32_code(op->reg);
            if (code < 0) code = get_reg16_code(op->reg);
            if (code < 0) {
                error(as, "invalid register for indirect addressing");
                return false;
            }
            emit_byte(as, code);
            return true;
        }

        case ADDR_REGISTER_IND_INC: {
            int code = get_reg32_code(op->reg);
            if (code < 0) code = get_reg16_code(op->reg);
            if (code < 0) {
                error(as, "invalid register for post-increment");
                return false;
            }
            emit_byte(as, 0x40 + code);
            return true;
        }

        case ADDR_REGISTER_IND_DEC: {
            int code = get_reg32_code(op->reg);
            if (code < 0) code = get_reg16_code(op->reg);
            if (code < 0) {
                error(as, "invalid register for pre-decrement");
                return false;
            }
            emit_byte(as, 0x48 + code);
            return true;
        }

        case ADDR_INDEXED: {
            int code = get_reg32_code(op->reg);
            if (code < 0) code = get_reg16_code(op->reg);
            if (code < 0) {
                error(as, "invalid register for indexed addressing");
                return false;
            }
            int disp = (int)op->value;
            if (disp >= -128 && disp <= 127) {
                emit_byte(as, 0x50 + code);  /* 8-bit displacement */
                emit_byte(as, (uint8_t)disp);
            } else {
                emit_byte(as, 0x58 + code);  /* 16-bit displacement */
                emit_word(as, (uint16_t)disp);
            }
            return true;
        }

        case ADDR_DIRECT: {
            int addr = (int)op->value;
            int addr_size = op->addr_size;
            if (addr_size == 0) {
                /* Auto-detect size */
                if (addr <= 0xFF) addr_size = 8;
                else if (addr <= 0xFFFF) addr_size = 16;
                else addr_size = 24;
            }
            switch (addr_size) {
                case 8:
                    emit_byte(as, 0xC0);
                    emit_byte(as, (uint8_t)addr);
                    break;
                case 16:
                    emit_byte(as, 0xD0);
                    emit_word(as, (uint16_t)addr);
                    break;
                case 24:
                default:
                    emit_byte(as, 0xE0);
                    emit_byte(as, addr & 0xFF);
                    emit_byte(as, (addr >> 8) & 0xFF);
                    emit_byte(as, (addr >> 16) & 0xFF);
                    break;
            }
            return true;
        }

        default:
            error(as, "unsupported addressing mode for memory operand");
            return false;
    }
}

/* ============== System Instructions ============== */

/* NOP */
static bool encode_nop(Assembler *as, Operand *ops, int count) {
    (void)ops; (void)count;
    emit_byte(as, 0x00);
    return true;
}

/* EI imm3 - Enable interrupts */
static bool encode_ei(Assembler *as, Operand *ops, int count) {
    int level = 7;
    if (count >= 1 && ops[0].mode == ADDR_IMMEDIATE) {
        level = (int)ops[0].value & 7;
    }
    emit_byte(as, 0x03);
    emit_byte(as, level);
    return true;
}

/* DI - Disable interrupts */
static bool encode_di(Assembler *as, Operand *ops, int count) {
    (void)ops; (void)count;
    emit_byte(as, 0x06);
    return true;
}

/* HALT */
static bool encode_halt(Assembler *as, Operand *ops, int count) {
    (void)ops; (void)count;
    emit_byte(as, 0x05);
    return true;
}

/* SCF - Set carry flag */
static bool encode_scf(Assembler *as, Operand *ops, int count) {
    (void)ops; (void)count;
    emit_byte(as, 0x0D);
    return true;
}

/* RCF - Reset carry flag */
static bool encode_rcf(Assembler *as, Operand *ops, int count) {
    (void)ops; (void)count;
    emit_byte(as, 0x0C);
    return true;
}

/* CCF - Complement carry flag */
static bool encode_ccf(Assembler *as, Operand *ops, int count) {
    (void)ops; (void)count;
    emit_byte(as, 0x0E);
    return true;
}

/* ZCF - Zero carry flag */
static bool encode_zcf(Assembler *as, Operand *ops, int count) {
    (void)ops; (void)count;
    emit_byte(as, 0x0F);
    return true;
}

/* ============== Stack Instructions ============== */

/* PUSH */
static bool encode_push(Assembler *as, Operand *ops, int count) {
    if (count < 1) {
        error(as, "PUSH requires an operand");
        return false;
    }

    if (ops[0].mode == ADDR_REGISTER) {
        int code;
        if (ops[0].size == SIZE_WORD) {
            code = get_reg16_code(ops[0].reg);
            if (code >= 0) {
                emit_byte(as, 0x28 + code);
                return true;
            }
        } else if (ops[0].size == SIZE_LONG) {
            code = get_reg32_code(ops[0].reg);
            if (code >= 0) {
                emit_byte(as, 0x30 + code);
                return true;
            }
        } else if (ops[0].size == SIZE_BYTE) {
            int prefix = get_reg8_prefix(ops[0].reg);
            if (prefix >= 0) {
                emit_byte(as, prefix);
                emit_byte(as, 0x14 + (get_reg8_code(ops[0].reg) & 1));
                return true;
            }
        }

        /* Special registers */
        if (ops[0].reg == REG_F) {
            emit_byte(as, 0x18);
            return true;
        }
        if (ops[0].reg == REG_A) {
            emit_byte(as, 0x19);
            return true;
        }
        if (ops[0].reg == REG_SR) {
            emit_byte(as, 0x02);
            return true;
        }
    }

    /* PUSH #imm (word) */
    if (ops[0].mode == ADDR_IMMEDIATE) {
        emit_byte(as, 0x09);  /* PUSH #imm16 */
        emit_word(as, (uint16_t)ops[0].value);
        return true;
    }

    error(as, "invalid PUSH operand");
    return false;
}

/* PUSHW - Push word immediate */
static bool encode_pushw(Assembler *as, Operand *ops, int count) {
    if (count < 1) {
        error(as, "PUSHW requires an operand");
        return false;
    }

    if (ops[0].mode == ADDR_IMMEDIATE) {
        emit_byte(as, 0x09);  /* PUSH #imm16 */
        emit_word(as, (uint16_t)ops[0].value);
        return true;
    }

    /* PUSHW (mem) - push word from memory */
    if (ops[0].mode == ADDR_REGISTER_IND || ops[0].mode == ADDR_INDEXED ||
        ops[0].mode == ADDR_DIRECT) {
        emit_byte(as, 0x90);  /* Word memory prefix */
        emit_mem_operand(as, &ops[0]);
        emit_byte(as, 0x04);  /* PUSH opcode */
        return true;
    }

    error(as, "invalid PUSHW operand");
    return false;
}

/* POP */
static bool encode_pop(Assembler *as, Operand *ops, int count) {
    if (count < 1) {
        error(as, "POP requires an operand");
        return false;
    }

    if (ops[0].mode == ADDR_REGISTER) {
        int code;
        if (ops[0].size == SIZE_WORD) {
            code = get_reg16_code(ops[0].reg);
            if (code >= 0) {
                emit_byte(as, 0x58 + code);
                return true;
            }
        } else if (ops[0].size == SIZE_LONG) {
            code = get_reg32_code(ops[0].reg);
            if (code >= 0) {
                emit_byte(as, 0x38 + code);
                return true;
            }
        }

        /* Special registers */
        if (ops[0].reg == REG_F) {
            emit_byte(as, 0x1A);
            return true;
        }
        if (ops[0].reg == REG_A) {
            emit_byte(as, 0x1B);
            return true;
        }
        if (ops[0].reg == REG_SR) {
            emit_byte(as, 0x03);
            return true;
        }
    }

    error(as, "invalid POP operand");
    return false;
}

/* LINK */
static bool encode_link(Assembler *as, Operand *ops, int count) {
    if (count < 2) {
        error(as, "LINK requires register and displacement");
        return false;
    }
    if (ops[0].mode != ADDR_REGISTER || ops[0].size != SIZE_LONG) {
        error(as, "LINK requires 32-bit register");
        return false;
    }
    int code = get_reg32_code(ops[0].reg);
    if (code < 0) {
        error(as, "invalid LINK register");
        return false;
    }
    emit_byte(as, 0xE8 + code);
    emit_byte(as, 0x0C);
    emit_word(as, (uint16_t)ops[1].value);
    return true;
}

/* UNLK */
static bool encode_unlk(Assembler *as, Operand *ops, int count) {
    if (count < 1) {
        error(as, "UNLK requires a register");
        return false;
    }
    if (ops[0].mode != ADDR_REGISTER || ops[0].size != SIZE_LONG) {
        error(as, "UNLK requires 32-bit register");
        return false;
    }
    int code = get_reg32_code(ops[0].reg);
    if (code < 0) {
        error(as, "invalid UNLK register");
        return false;
    }
    emit_byte(as, 0xE8 + code);
    emit_byte(as, 0x0D);
    return true;
}

/* ============== Control Flow Instructions ============== */

/* RET */
static bool encode_ret(Assembler *as, Operand *ops, int count) {
    /* Check for conditional return */
    if (count >= 1 && ops[0].mode == ADDR_CONDITION) {
        emit_byte(as, 0xB0 + get_cc_code(ops[0].value));
        return true;
    }
    emit_byte(as, 0x0E);
    return true;
}

/* RETI */
static bool encode_reti(Assembler *as, Operand *ops, int count) {
    (void)ops; (void)count;
    emit_byte(as, 0x07);
    return true;
}

/* RETD d16 */
static bool encode_retd(Assembler *as, Operand *ops, int count) {
    if (count < 1) {
        error(as, "RETD requires displacement");
        return false;
    }
    emit_byte(as, 0x0F);
    emit_word(as, (uint16_t)ops[0].value);
    return true;
}

/* SWI imm3 */
static bool encode_swi(Assembler *as, Operand *ops, int count) {
    if (count < 1) {
        error(as, "SWI requires interrupt number");
        return false;
    }
    int n = (int)ops[0].value & 7;
    emit_byte(as, 0xF8 + n);
    return true;
}

/* JP - Jump */
static bool encode_jp(Assembler *as, Operand *ops, int count) {
    if (count < 1) {
        error(as, "JP requires an operand");
        return false;
    }

    int cc = CC_T;
    Operand *target = &ops[0];

    if (count >= 2 && ops[0].mode == ADDR_CONDITION) {
        cc = (int)ops[0].value;
        target = &ops[1];
    }

    /* JP cc, nn - conditional absolute jump */
    if (target->mode == ADDR_IMMEDIATE) {
        int addr = (int)target->value;
        emit_byte(as, 0xB0 + get_cc_code(cc));
        emit_byte(as, addr & 0xFF);
        emit_byte(as, (addr >> 8) & 0xFF);
        emit_byte(as, (addr >> 16) & 0xFF);
        return true;
    }

    /* JP [cc,] (mem) - indirect jump */
    if (target->mode == ADDR_REGISTER_IND || target->mode == ADDR_INDEXED ||
        target->mode == ADDR_DIRECT) {
        emit_byte(as, 0xB4);
        emit_mem_operand(as, target);
        emit_byte(as, 0xD0 + get_cc_code(cc));
        return true;
    }

    error(as, "invalid JP operand");
    return false;
}

/* JR - Jump relative */
static bool encode_jr(Assembler *as, Operand *ops, int count) {
    if (count < 1) {
        error(as, "JR requires an operand");
        return false;
    }

    int cc = CC_T;
    Operand *target = &ops[0];

    if (count >= 2 && ops[0].mode == ADDR_CONDITION) {
        cc = (int)ops[0].value;
        target = &ops[1];
    }

    if (target->mode != ADDR_IMMEDIATE) {
        error(as, "JR requires an immediate target");
        return false;
    }

    int64_t offset = target->value - (as->pc + 2);

    /* On pass 1, emit bytes even if offset unknown (forward reference) */
    if (as->pass == 1 || (offset >= -128 && offset <= 127)) {
        emit_byte(as, 0x60 + get_cc_code(cc));
        emit_byte(as, (uint8_t)offset);
    } else {
        error(as, "JR offset out of range (use JRL for longer jumps)");
        return false;
    }

    return true;
}

/* JRL - Jump relative long */
static bool encode_jrl(Assembler *as, Operand *ops, int count) {
    if (count < 1) {
        error(as, "JRL requires an operand");
        return false;
    }

    int cc = CC_T;
    Operand *target = &ops[0];

    if (count >= 2 && ops[0].mode == ADDR_CONDITION) {
        cc = (int)ops[0].value;
        target = &ops[1];
    }

    if (target->mode != ADDR_IMMEDIATE) {
        error(as, "JRL requires an immediate target");
        return false;
    }

    int64_t offset = target->value - (as->pc + 3);

    emit_byte(as, 0x70 + get_cc_code(cc));
    emit_word(as, (uint16_t)offset);

    return true;
}

/* CALL */
static bool encode_call(Assembler *as, Operand *ops, int count) {
    if (count < 1) {
        error(as, "CALL requires an operand");
        return false;
    }

    int cc = CC_T;
    Operand *target = &ops[0];

    if (count >= 2 && ops[0].mode == ADDR_CONDITION) {
        cc = (int)ops[0].value;
        target = &ops[1];
    }

    if (target->mode == ADDR_IMMEDIATE) {
        emit_byte(as, 0x9A + get_cc_code(cc));
        emit_byte(as, target->value & 0xFF);
        emit_byte(as, (target->value >> 8) & 0xFF);
        emit_byte(as, (target->value >> 16) & 0xFF);
        return true;
    }

    /* CALL [cc,] reg32 - call to address in register */
    if (target->mode == ADDR_REGISTER && target->size == SIZE_LONG) {
        int code = get_reg32_code(target->reg);
        if (code >= 0) {
            emit_byte(as, 0xE8 + code);
            emit_byte(as, 0x90 + get_cc_code(cc));
            return true;
        }
    }

    /* CALL [cc,] (mem) - indirect call */
    if (target->mode == ADDR_REGISTER_IND || target->mode == ADDR_INDEXED ||
        target->mode == ADDR_DIRECT) {
        emit_byte(as, 0xB4);
        emit_mem_operand(as, target);
        /* CALL cc,(mem) uses 0xD1+cc where cc=8 for unconditional */
        emit_byte(as, 0xD1 + get_cc_code(cc));
        return true;
    }

    error(as, "invalid CALL operand");
    return false;
}

/* CALR - Call relative */
static bool encode_calr(Assembler *as, Operand *ops, int count) {
    if (count < 1) {
        error(as, "CALR requires an operand");
        return false;
    }

    if (ops[0].mode != ADDR_IMMEDIATE) {
        error(as, "CALR requires an immediate target");
        return false;
    }

    int64_t offset = ops[0].value - (as->pc + 3);
    emit_byte(as, 0x1D);
    emit_word(as, (uint16_t)offset);

    return true;
}

/* DJNZ - Decrement and jump if not zero */
static bool encode_djnz(Assembler *as, Operand *ops, int count) {
    if (count < 2) {
        error(as, "DJNZ requires register and target");
        return false;
    }

    if (ops[0].mode != ADDR_REGISTER) {
        error(as, "DJNZ first operand must be a register");
        return false;
    }

    int64_t offset = ops[1].value - (as->pc + 3);

    if (ops[0].size == SIZE_BYTE) {
        int code = get_reg8_code(ops[0].reg);
        if (code >= 0) {
            emit_byte(as, 0xC8 + (code >> 1));
            emit_byte(as, 0x1C + (code & 1));
            emit_byte(as, (uint8_t)offset);
            return true;
        }
    } else if (ops[0].size == SIZE_WORD) {
        int code = get_reg16_code(ops[0].reg);
        if (code >= 0) {
            emit_byte(as, 0xD8 + code);
            emit_byte(as, 0x1C);
            emit_byte(as, (uint8_t)offset);
            return true;
        }
    }

    error(as, "invalid DJNZ register");
    return false;
}

/* ============== Data Movement Instructions ============== */

/* LD - Load (complex, many forms) */
static bool encode_ld(Assembler *as, Operand *ops, int count) {
    if (count < 2) {
        error(as, "LD requires two operands");
        return false;
    }

    Operand *dst = &ops[0];
    Operand *src = &ops[1];

    /* Short form: LD r8, imm (0x20-0x27) */
    if (dst->mode == ADDR_REGISTER && dst->size == SIZE_BYTE &&
        src->mode == ADDR_IMMEDIATE) {
        int code = get_reg8_code(dst->reg);
        if (code >= 0) {
            /* Use short form */
            emit_byte(as, 0x20 + code);
            emit_byte(as, (uint8_t)src->value);
            return true;
        }
    }

    /* LD reg, imm */
    if (dst->mode == ADDR_REGISTER && src->mode == ADDR_IMMEDIATE) {
        if (dst->size == SIZE_BYTE) {
            int code = get_reg8_code(dst->reg);
            if (code >= 0) {
                emit_byte(as, 0xC8 + (code >> 1));
                emit_byte(as, 0x30 + (code & 1));
                emit_byte(as, (uint8_t)src->value);
                return true;
            }
        } else if (dst->size == SIZE_WORD) {
            int code = get_reg16_code(dst->reg);
            if (code >= 0) {
                emit_byte(as, 0xD8 + code);
                emit_byte(as, 0x30);
                emit_word(as, (uint16_t)src->value);
                return true;
            }
        } else if (dst->size == SIZE_LONG) {
            int code = get_reg32_code(dst->reg);
            if (code >= 0) {
                emit_byte(as, 0xE8 + code);
                emit_byte(as, 0x30);
                emit_long(as, (uint32_t)src->value);
                return true;
            }
        }
    }

    /* LD reg, reg */
    if (dst->mode == ADDR_REGISTER && src->mode == ADDR_REGISTER) {
        if (dst->size == SIZE_BYTE && src->size == SIZE_BYTE) {
            int dcode = get_reg8_code(dst->reg);
            int scode = get_reg8_code(src->reg);
            if (dcode >= 0 && scode >= 0) {
                emit_byte(as, 0xC8 + (scode >> 1));
                emit_byte(as, 0x20 + ((scode & 1) << 3) + ((dcode >> 1) << 1) + (dcode & 1));
                return true;
            }
        } else if (dst->size == SIZE_WORD && src->size == SIZE_WORD) {
            int dcode = get_reg16_code(dst->reg);
            int scode = get_reg16_code(src->reg);
            if (dcode >= 0 && scode >= 0) {
                emit_byte(as, 0xD8 + scode);
                emit_byte(as, 0x28 + dcode);
                return true;
            }
        } else if (dst->size == SIZE_LONG && src->size == SIZE_LONG) {
            int dcode = get_reg32_code(dst->reg);
            int scode = get_reg32_code(src->reg);
            if (dcode >= 0 && scode >= 0) {
                emit_byte(as, 0xE8 + scode);
                emit_byte(as, 0x28 + dcode);
                return true;
            }
        }
    }

    /* LD reg, (mem) */
    if (dst->mode == ADDR_REGISTER &&
        (src->mode == ADDR_REGISTER_IND || src->mode == ADDR_INDEXED ||
         src->mode == ADDR_DIRECT || src->mode == ADDR_REGISTER_IND_INC)) {

        if (dst->size == SIZE_BYTE) {
            int code = get_reg8_code(dst->reg);
            if (code >= 0) {
                emit_byte(as, 0x80 + (code >> 1));
                emit_mem_operand(as, src);
                emit_byte(as, 0x20 + (code & 1));
                return true;
            }
        } else if (dst->size == SIZE_WORD) {
            int code = get_reg16_code(dst->reg);
            if (code >= 0) {
                emit_byte(as, 0x90);
                emit_mem_operand(as, src);
                emit_byte(as, 0x20 + code);
                return true;
            }
        } else if (dst->size == SIZE_LONG) {
            int code = get_reg32_code(dst->reg);
            if (code >= 0) {
                emit_byte(as, 0xA0);
                emit_mem_operand(as, src);
                emit_byte(as, 0x20 + code);
                return true;
            }
        }
    }

    /* LD (mem), reg */
    if ((dst->mode == ADDR_REGISTER_IND || dst->mode == ADDR_INDEXED ||
         dst->mode == ADDR_DIRECT || dst->mode == ADDR_REGISTER_IND_DEC ||
         dst->mode == ADDR_REGISTER_IND_INC) &&
        src->mode == ADDR_REGISTER) {

        if (src->size == SIZE_BYTE) {
            int code = get_reg8_code(src->reg);
            if (code >= 0) {
                emit_byte(as, 0x80 + (code >> 1));
                emit_mem_operand(as, dst);
                emit_byte(as, 0x48 + (code & 1));
                return true;
            }
        } else if (src->size == SIZE_WORD) {
            int code = get_reg16_code(src->reg);
            if (code >= 0) {
                emit_byte(as, 0x90);
                emit_mem_operand(as, dst);
                emit_byte(as, 0x48 + code);
                return true;
            }
        } else if (src->size == SIZE_LONG) {
            int code = get_reg32_code(src->reg);
            if (code >= 0) {
                emit_byte(as, 0xA0);
                emit_mem_operand(as, dst);
                emit_byte(as, 0x48 + code);
                return true;
            }
        }
    }

    /* LD (mem), imm */
    if ((dst->mode == ADDR_REGISTER_IND || dst->mode == ADDR_INDEXED ||
         dst->mode == ADDR_DIRECT || dst->mode == ADDR_REGISTER_IND_INC ||
         dst->mode == ADDR_REGISTER_IND_DEC) && src->mode == ADDR_IMMEDIATE) {
        emit_byte(as, 0x80);
        emit_mem_operand(as, dst);
        emit_byte(as, 0x00);
        emit_byte(as, (uint8_t)src->value);
        return true;
    }

    error(as, "unsupported LD operand combination");
    return false;
}

/* LDA - Load address */
static bool encode_lda(Assembler *as, Operand *ops, int count) {
    if (count < 2) {
        error(as, "LDA requires two operands");
        return false;
    }

    Operand *dst = &ops[0];
    Operand *src = &ops[1];

    if (dst->mode != ADDR_REGISTER || dst->size != SIZE_LONG) {
        error(as, "LDA destination must be 32-bit register");
        return false;
    }

    int dcode = get_reg32_code(dst->reg);
    if (dcode < 0) {
        error(as, "invalid LDA destination register");
        return false;
    }

    /* LDA xrr, (mem) */
    if (src->mode == ADDR_REGISTER_IND || src->mode == ADDR_INDEXED ||
        src->mode == ADDR_DIRECT || src->mode == ADDR_REGISTER_IND_INC) {
        emit_byte(as, 0xF5);
        emit_mem_operand(as, src);
        emit_byte(as, 0x30 + dcode);
        return true;
    }

    /* LDA xrr, imm - treat immediate as direct address */
    if (src->mode == ADDR_IMMEDIATE) {
        /* Encode as LDA xrr, (imm) */
        Operand direct_op = *src;
        direct_op.mode = ADDR_DIRECT;
        emit_byte(as, 0xF5);
        emit_mem_operand(as, &direct_op);
        emit_byte(as, 0x30 + dcode);
        return true;
    }

    /* LDA xrr, xrr + offset (without parentheses) - treat as indexed */
    if (src->mode == ADDR_REGISTER && src->size == SIZE_LONG && count >= 3) {
        /* Check if there's an offset operand */
        if (ops[2].mode == ADDR_IMMEDIATE) {
            Operand indexed_op;
            indexed_op.mode = ADDR_INDEXED;
            indexed_op.reg = src->reg;
            indexed_op.size = src->size;
            indexed_op.value = ops[2].value;
            indexed_op.value_known = ops[2].value_known;
            emit_byte(as, 0xF5);
            emit_mem_operand(as, &indexed_op);
            emit_byte(as, 0x30 + dcode);
            return true;
        }
    }

    /* LDA xrr, xrr (treat register as base for indirect) */
    if (src->mode == ADDR_REGISTER && src->size == SIZE_LONG) {
        Operand indirect_op;
        indirect_op.mode = ADDR_REGISTER_IND;
        indirect_op.reg = src->reg;
        indirect_op.size = src->size;
        emit_byte(as, 0xF5);
        emit_mem_operand(as, &indirect_op);
        emit_byte(as, 0x30 + dcode);
        return true;
    }

    error(as, "unsupported LDA operand combination");
    return false;
}

/* LDC - Load control register (stub - complex) */
static bool encode_ldc(Assembler *as, Operand *ops, int count) {
    /* LDC is complex - depends on control register names */
    /* For now, provide basic support */
    (void)ops; (void)count;
    error(as, "LDC not yet fully implemented - use DB directive");
    return false;
}

/* LDI - Load and increment */
static bool encode_ldi(Assembler *as, Operand *ops, int count) {
    (void)ops; (void)count;
    emit_byte(as, 0x85);
    emit_byte(as, 0x10);
    return true;
}

/* LDIR - Load, increment and repeat */
static bool encode_ldir(Assembler *as, Operand *ops, int count) {
    (void)ops; (void)count;
    emit_byte(as, 0x85);
    emit_byte(as, 0x11);
    return true;
}

/* LDDR - Load, decrement and repeat */
static bool encode_lddr(Assembler *as, Operand *ops, int count) {
    (void)ops; (void)count;
    emit_byte(as, 0x85);
    emit_byte(as, 0x13);
    return true;
}

/* LDIW - Load word and increment */
static bool encode_ldiw(Assembler *as, Operand *ops, int count) {
    (void)ops; (void)count;
    emit_byte(as, 0x95);
    emit_byte(as, 0x10);
    return true;
}

/* LDIRW - Load word, increment and repeat */
static bool encode_ldirw(Assembler *as, Operand *ops, int count) {
    (void)ops; (void)count;
    emit_byte(as, 0x95);
    emit_byte(as, 0x11);
    return true;
}

/* LDDRW - Load word, decrement and repeat */
static bool encode_lddrw(Assembler *as, Operand *ops, int count) {
    (void)ops; (void)count;
    emit_byte(as, 0x95);
    emit_byte(as, 0x13);
    return true;
}

/* LDW - Load word (word-size LD variant) */
static bool encode_ldw(Assembler *as, Operand *ops, int count) {
    if (count < 2) {
        error(as, "LDW requires two operands");
        return false;
    }

    Operand *dst = &ops[0];
    Operand *src = &ops[1];

    /* LDW (mem), imm16 */
    if ((dst->mode == ADDR_DIRECT || dst->mode == ADDR_REGISTER_IND ||
         dst->mode == ADDR_INDEXED) && src->mode == ADDR_IMMEDIATE) {
        emit_byte(as, 0x90);
        emit_mem_operand(as, dst);
        emit_byte(as, 0x00);
        emit_word(as, (uint16_t)src->value);
        return true;
    }

    /* LDW reg16, (mem) */
    if (dst->mode == ADDR_REGISTER && dst->size == SIZE_WORD &&
        (src->mode == ADDR_DIRECT || src->mode == ADDR_REGISTER_IND ||
         src->mode == ADDR_INDEXED)) {
        int code = get_reg16_code(dst->reg);
        if (code >= 0) {
            emit_byte(as, 0x90);
            emit_mem_operand(as, src);
            emit_byte(as, 0x20 + code);
            return true;
        }
    }

    /* LDW (mem), reg16 */
    if ((dst->mode == ADDR_DIRECT || dst->mode == ADDR_REGISTER_IND ||
         dst->mode == ADDR_INDEXED) && src->mode == ADDR_REGISTER && src->size == SIZE_WORD) {
        int code = get_reg16_code(src->reg);
        if (code >= 0) {
            emit_byte(as, 0x90);
            emit_mem_operand(as, dst);
            emit_byte(as, 0x48 + code);
            return true;
        }
    }

    /* LDW (reg+), imm16 - post-increment destination */
    if (dst->mode == ADDR_REGISTER_IND_INC && src->mode == ADDR_IMMEDIATE) {
        emit_byte(as, 0x90);
        emit_mem_operand(as, dst);
        emit_byte(as, 0x00);
        emit_word(as, (uint16_t)src->value);
        return true;
    }

    error(as, "unsupported LDW operand combination");
    return false;
}

/* EX - Exchange */
static bool encode_ex(Assembler *as, Operand *ops, int count) {
    if (count < 2) {
        error(as, "EX requires two operands");
        return false;
    }

    /* EX (mem), reg */
    if ((ops[0].mode == ADDR_REGISTER_IND || ops[0].mode == ADDR_INDEXED ||
         ops[0].mode == ADDR_DIRECT) && ops[1].mode == ADDR_REGISTER) {

        if (ops[1].size == SIZE_BYTE) {
            int code = get_reg8_code(ops[1].reg);
            if (code >= 0) {
                emit_byte(as, 0x80 + (code >> 1));
                emit_mem_operand(as, &ops[0]);
                emit_byte(as, 0x30 + (code & 1));
                return true;
            }
        } else if (ops[1].size == SIZE_WORD) {
            int code = get_reg16_code(ops[1].reg);
            if (code >= 0) {
                emit_byte(as, 0x90);
                emit_mem_operand(as, &ops[0]);
                emit_byte(as, 0x30 + code);
                return true;
            }
        } else if (ops[1].size == SIZE_LONG) {
            int code = get_reg32_code(ops[1].reg);
            if (code >= 0) {
                emit_byte(as, 0xA0);
                emit_mem_operand(as, &ops[0]);
                emit_byte(as, 0x30 + code);
                return true;
            }
        }
    }

    /* EX reg, reg */
    if (ops[0].mode == ADDR_REGISTER && ops[1].mode == ADDR_REGISTER) {
        if (ops[0].size == SIZE_BYTE && ops[1].size == SIZE_BYTE) {
            int code0 = get_reg8_code(ops[0].reg);
            int code1 = get_reg8_code(ops[1].reg);
            if (code0 >= 0 && code1 >= 0) {
                emit_byte(as, 0xC8 + (code1 >> 1));
                emit_byte(as, 0x38 + ((code1 & 1) << 3) + ((code0 >> 1) << 1) + (code0 & 1));
                return true;
            }
        }
        if (ops[0].size == SIZE_WORD && ops[1].size == SIZE_WORD) {
            int code0 = get_reg16_code(ops[0].reg);
            int code1 = get_reg16_code(ops[1].reg);
            if (code0 >= 0 && code1 >= 0) {
                emit_byte(as, 0xD8 + code1);
                emit_byte(as, 0x38 + code0);
                return true;
            }
        }
    }

    error(as, "unsupported EX operand combination");
    return false;
}

/* ============== Arithmetic Instructions ============== */

/* ADD */
static bool encode_add(Assembler *as, Operand *ops, int count) {
    if (count < 2) {
        error(as, "ADD requires two operands");
        return false;
    }

    Operand *dst = &ops[0];
    Operand *src = &ops[1];

    /* ADD reg, imm */
    if (dst->mode == ADDR_REGISTER && src->mode == ADDR_IMMEDIATE) {
        if (dst->size == SIZE_BYTE) {
            int code = get_reg8_code(dst->reg);
            if (code >= 0) {
                emit_byte(as, 0xC8 + (code >> 1));
                emit_byte(as, 0xC8 + (code & 1));
                emit_byte(as, (uint8_t)src->value);
                return true;
            }
        } else if (dst->size == SIZE_WORD) {
            int code = get_reg16_code(dst->reg);
            if (code >= 0) {
                emit_byte(as, 0xD8 + code);
                emit_byte(as, 0xC8);
                emit_word(as, (uint16_t)src->value);
                return true;
            }
        } else if (dst->size == SIZE_LONG) {
            int code = get_reg32_code(dst->reg);
            if (code >= 0) {
                emit_byte(as, 0xE8 + code);
                emit_byte(as, 0xC8);
                emit_long(as, (uint32_t)src->value);
                return true;
            }
        }
    }

    /* ADD reg, reg */
    if (dst->mode == ADDR_REGISTER && src->mode == ADDR_REGISTER) {
        if (dst->size == SIZE_BYTE && src->size == SIZE_BYTE) {
            int dcode = get_reg8_code(dst->reg);
            int scode = get_reg8_code(src->reg);
            if (dcode >= 0 && scode >= 0) {
                emit_byte(as, 0xC8 + (scode >> 1));
                emit_byte(as, 0x80 + ((scode & 1) << 3) + ((dcode >> 1) << 1) + (dcode & 1));
                return true;
            }
        } else if (dst->size == SIZE_WORD && src->size == SIZE_WORD) {
            int dcode = get_reg16_code(dst->reg);
            int scode = get_reg16_code(src->reg);
            if (dcode >= 0 && scode >= 0) {
                emit_byte(as, 0xD8 + scode);
                emit_byte(as, 0x80 + dcode);
                return true;
            }
        } else if (dst->size == SIZE_LONG && src->size == SIZE_LONG) {
            int dcode = get_reg32_code(dst->reg);
            int scode = get_reg32_code(src->reg);
            if (dcode >= 0 && scode >= 0) {
                emit_byte(as, 0xE8 + scode);
                emit_byte(as, 0x80 + dcode);
                return true;
            }
        }
    }

    /* ADD reg, (mem) */
    if (dst->mode == ADDR_REGISTER &&
        (src->mode == ADDR_REGISTER_IND || src->mode == ADDR_INDEXED ||
         src->mode == ADDR_DIRECT || src->mode == ADDR_REGISTER_IND_INC ||
         src->mode == ADDR_REGISTER_IND_DEC)) {
        if (dst->size == SIZE_BYTE) {
            int code = get_reg8_code(dst->reg);
            if (code >= 0) {
                emit_byte(as, 0x80 + (code >> 1));
                emit_mem_operand(as, src);
                emit_byte(as, 0x00 + (code & 1));
                return true;
            }
        } else if (dst->size == SIZE_WORD) {
            int code = get_reg16_code(dst->reg);
            if (code >= 0) {
                emit_byte(as, 0x90);
                emit_mem_operand(as, src);
                emit_byte(as, 0x00 + code);
                return true;
            }
        } else if (dst->size == SIZE_LONG) {
            int code = get_reg32_code(dst->reg);
            if (code >= 0) {
                emit_byte(as, 0xA0);
                emit_mem_operand(as, src);
                emit_byte(as, 0x00 + code);
                return true;
            }
        }
    }

    /* ADD (mem), reg */
    if ((dst->mode == ADDR_REGISTER_IND || dst->mode == ADDR_INDEXED ||
         dst->mode == ADDR_DIRECT) && src->mode == ADDR_REGISTER) {
        if (src->size == SIZE_BYTE) {
            int code = get_reg8_code(src->reg);
            if (code >= 0) {
                emit_byte(as, 0x80 + (code >> 1));
                emit_mem_operand(as, dst);
                emit_byte(as, 0x08 + (code & 1));
                return true;
            }
        } else if (src->size == SIZE_WORD) {
            int code = get_reg16_code(src->reg);
            if (code >= 0) {
                emit_byte(as, 0x90);
                emit_mem_operand(as, dst);
                emit_byte(as, 0x08 + code);
                return true;
            }
        } else if (src->size == SIZE_LONG) {
            int code = get_reg32_code(src->reg);
            if (code >= 0) {
                emit_byte(as, 0xA0);
                emit_mem_operand(as, dst);
                emit_byte(as, 0x08 + code);
                return true;
            }
        }
    }

    error(as, "unsupported ADD operand combination");
    return false;
}

/* ADC - Add with carry */
static bool encode_adc(Assembler *as, Operand *ops, int count) {
    if (count < 2) {
        error(as, "ADC requires two operands");
        return false;
    }

    Operand *dst = &ops[0];
    Operand *src = &ops[1];

    /* ADC reg, imm */
    if (dst->mode == ADDR_REGISTER && src->mode == ADDR_IMMEDIATE) {
        if (dst->size == SIZE_BYTE) {
            int code = get_reg8_code(dst->reg);
            if (code >= 0) {
                emit_byte(as, 0xC8 + (code >> 1));
                emit_byte(as, 0xC0 + (code & 1));
                emit_byte(as, (uint8_t)src->value);
                return true;
            }
        } else if (dst->size == SIZE_WORD) {
            int code = get_reg16_code(dst->reg);
            if (code >= 0) {
                emit_byte(as, 0xD8 + code);
                emit_byte(as, 0xC0);
                emit_word(as, (uint16_t)src->value);
                return true;
            }
        } else if (dst->size == SIZE_LONG) {
            int code = get_reg32_code(dst->reg);
            if (code >= 0) {
                emit_byte(as, 0xE8 + code);
                emit_byte(as, 0xC0);
                emit_long(as, (uint32_t)src->value);
                return true;
            }
        }
    }

    /* ADC reg, reg */
    if (dst->mode == ADDR_REGISTER && src->mode == ADDR_REGISTER) {
        if (dst->size == SIZE_BYTE && src->size == SIZE_BYTE) {
            int dcode = get_reg8_code(dst->reg);
            int scode = get_reg8_code(src->reg);
            if (dcode >= 0 && scode >= 0) {
                emit_byte(as, 0xC8 + (scode >> 1));
                emit_byte(as, 0x88 + ((scode & 1) << 3) + ((dcode >> 1) << 1) + (dcode & 1));
                return true;
            }
        }
        if (dst->size == SIZE_WORD && src->size == SIZE_WORD) {
            int dcode = get_reg16_code(dst->reg);
            int scode = get_reg16_code(src->reg);
            if (dcode >= 0 && scode >= 0) {
                emit_byte(as, 0xD8 + scode);
                emit_byte(as, 0x88 + dcode);
                return true;
            }
        }
        if (dst->size == SIZE_LONG && src->size == SIZE_LONG) {
            int dcode = get_reg32_code(dst->reg);
            int scode = get_reg32_code(src->reg);
            if (dcode >= 0 && scode >= 0) {
                emit_byte(as, 0xE8 + scode);
                emit_byte(as, 0x88 + dcode);
                return true;
            }
        }
    }

    /* ADC reg, (mem) */
    if (dst->mode == ADDR_REGISTER &&
        (src->mode == ADDR_REGISTER_IND || src->mode == ADDR_INDEXED ||
         src->mode == ADDR_DIRECT)) {
        if (dst->size == SIZE_BYTE) {
            int code = get_reg8_code(dst->reg);
            if (code >= 0) {
                emit_byte(as, 0x80 + (code >> 1));
                emit_mem_operand(as, src);
                emit_byte(as, 0x01 + (code & 1));
                return true;
            }
        } else if (dst->size == SIZE_WORD) {
            int code = get_reg16_code(dst->reg);
            if (code >= 0) {
                emit_byte(as, 0x90);
                emit_mem_operand(as, src);
                emit_byte(as, 0x10 + code);
                return true;
            }
        } else if (dst->size == SIZE_LONG) {
            int code = get_reg32_code(dst->reg);
            if (code >= 0) {
                emit_byte(as, 0xA0);
                emit_mem_operand(as, src);
                emit_byte(as, 0x10 + code);
                return true;
            }
        }
    }

    /* ADC (mem), reg */
    if ((dst->mode == ADDR_REGISTER_IND || dst->mode == ADDR_INDEXED ||
         dst->mode == ADDR_DIRECT) && src->mode == ADDR_REGISTER) {
        if (src->size == SIZE_BYTE) {
            int code = get_reg8_code(src->reg);
            if (code >= 0) {
                emit_byte(as, 0x80 + (code >> 1));
                emit_mem_operand(as, dst);
                emit_byte(as, 0x09 + (code & 1));
                return true;
            }
        } else if (src->size == SIZE_WORD) {
            int code = get_reg16_code(src->reg);
            if (code >= 0) {
                emit_byte(as, 0x90);
                emit_mem_operand(as, dst);
                emit_byte(as, 0x18 + code);
                return true;
            }
        } else if (src->size == SIZE_LONG) {
            int code = get_reg32_code(src->reg);
            if (code >= 0) {
                emit_byte(as, 0xA0);
                emit_mem_operand(as, dst);
                emit_byte(as, 0x18 + code);
                return true;
            }
        }
    }

    error(as, "unsupported ADC operand combination");
    return false;
}

/* SUB */
static bool encode_sub(Assembler *as, Operand *ops, int count) {
    if (count < 2) {
        error(as, "SUB requires two operands");
        return false;
    }

    Operand *dst = &ops[0];
    Operand *src = &ops[1];

    /* SUB reg, imm */
    if (dst->mode == ADDR_REGISTER && src->mode == ADDR_IMMEDIATE) {
        if (dst->size == SIZE_BYTE) {
            int code = get_reg8_code(dst->reg);
            if (code >= 0) {
                emit_byte(as, 0xC8 + (code >> 1));
                emit_byte(as, 0xCA + (code & 1));
                emit_byte(as, (uint8_t)src->value);
                return true;
            }
        } else if (dst->size == SIZE_WORD) {
            int code = get_reg16_code(dst->reg);
            if (code >= 0) {
                emit_byte(as, 0xD8 + code);
                emit_byte(as, 0xCA);
                emit_word(as, (uint16_t)src->value);
                return true;
            }
        } else if (dst->size == SIZE_LONG) {
            int code = get_reg32_code(dst->reg);
            if (code >= 0) {
                emit_byte(as, 0xE8 + code);
                emit_byte(as, 0xCA);
                emit_long(as, (uint32_t)src->value);
                return true;
            }
        }
    }

    /* SUB reg, reg */
    if (dst->mode == ADDR_REGISTER && src->mode == ADDR_REGISTER) {
        if (dst->size == SIZE_BYTE && src->size == SIZE_BYTE) {
            int dcode = get_reg8_code(dst->reg);
            int scode = get_reg8_code(src->reg);
            if (dcode >= 0 && scode >= 0) {
                emit_byte(as, 0xC8 + (scode >> 1));
                emit_byte(as, 0x90 + ((scode & 1) << 3) + ((dcode >> 1) << 1) + (dcode & 1));
                return true;
            }
        } else if (dst->size == SIZE_WORD && src->size == SIZE_WORD) {
            int dcode = get_reg16_code(dst->reg);
            int scode = get_reg16_code(src->reg);
            if (dcode >= 0 && scode >= 0) {
                emit_byte(as, 0xD8 + scode);
                emit_byte(as, 0x90 + dcode);
                return true;
            }
        } else if (dst->size == SIZE_LONG && src->size == SIZE_LONG) {
            int dcode = get_reg32_code(dst->reg);
            int scode = get_reg32_code(src->reg);
            if (dcode >= 0 && scode >= 0) {
                emit_byte(as, 0xE8 + scode);
                emit_byte(as, 0x90 + dcode);
                return true;
            }
        }
    }

    /* SUB reg, (mem) */
    if (dst->mode == ADDR_REGISTER &&
        (src->mode == ADDR_REGISTER_IND || src->mode == ADDR_INDEXED ||
         src->mode == ADDR_DIRECT)) {
        if (dst->size == SIZE_BYTE) {
            int code = get_reg8_code(dst->reg);
            if (code >= 0) {
                emit_byte(as, 0x80 + (code >> 1));
                emit_mem_operand(as, src);
                emit_byte(as, 0x02 + (code & 1));
                return true;
            }
        } else if (dst->size == SIZE_WORD) {
            int code = get_reg16_code(dst->reg);
            if (code >= 0) {
                emit_byte(as, 0x90);
                emit_mem_operand(as, src);
                emit_byte(as, 0x20 + code);
                return true;
            }
        } else if (dst->size == SIZE_LONG) {
            int code = get_reg32_code(dst->reg);
            if (code >= 0) {
                emit_byte(as, 0xA0);
                emit_mem_operand(as, src);
                emit_byte(as, 0x20 + code);
                return true;
            }
        }
    }

    /* SUB (mem), reg */
    if ((dst->mode == ADDR_REGISTER_IND || dst->mode == ADDR_INDEXED ||
         dst->mode == ADDR_DIRECT) && src->mode == ADDR_REGISTER) {
        if (src->size == SIZE_BYTE) {
            int code = get_reg8_code(src->reg);
            if (code >= 0) {
                emit_byte(as, 0x80 + (code >> 1));
                emit_mem_operand(as, dst);
                emit_byte(as, 0x0A + (code & 1));
                return true;
            }
        } else if (src->size == SIZE_WORD) {
            int code = get_reg16_code(src->reg);
            if (code >= 0) {
                emit_byte(as, 0x90);
                emit_mem_operand(as, dst);
                emit_byte(as, 0x28 + code);
                return true;
            }
        } else if (src->size == SIZE_LONG) {
            int code = get_reg32_code(src->reg);
            if (code >= 0) {
                emit_byte(as, 0xA0);
                emit_mem_operand(as, dst);
                emit_byte(as, 0x28 + code);
                return true;
            }
        }
    }

    error(as, "unsupported SUB operand combination");
    return false;
}

/* SBC - Subtract with carry */
static bool encode_sbc(Assembler *as, Operand *ops, int count) {
    if (count < 2) {
        error(as, "SBC requires two operands");
        return false;
    }

    Operand *dst = &ops[0];
    Operand *src = &ops[1];

    /* SBC reg, imm */
    if (dst->mode == ADDR_REGISTER && src->mode == ADDR_IMMEDIATE) {
        if (dst->size == SIZE_BYTE) {
            int code = get_reg8_code(dst->reg);
            if (code >= 0) {
                emit_byte(as, 0xC8 + (code >> 1));
                emit_byte(as, 0xC2 + (code & 1));
                emit_byte(as, (uint8_t)src->value);
                return true;
            }
        } else if (dst->size == SIZE_WORD) {
            int code = get_reg16_code(dst->reg);
            if (code >= 0) {
                emit_byte(as, 0xD8 + code);
                emit_byte(as, 0xC2);
                emit_word(as, (uint16_t)src->value);
                return true;
            }
        }
    }

    /* SBC reg, reg */
    if (dst->mode == ADDR_REGISTER && src->mode == ADDR_REGISTER) {
        if (dst->size == SIZE_BYTE && src->size == SIZE_BYTE) {
            int dcode = get_reg8_code(dst->reg);
            int scode = get_reg8_code(src->reg);
            if (dcode >= 0 && scode >= 0) {
                emit_byte(as, 0xC8 + (scode >> 1));
                emit_byte(as, 0x98 + ((scode & 1) << 3) + ((dcode >> 1) << 1) + (dcode & 1));
                return true;
            }
        }
        if (dst->size == SIZE_WORD && src->size == SIZE_WORD) {
            int dcode = get_reg16_code(dst->reg);
            int scode = get_reg16_code(src->reg);
            if (dcode >= 0 && scode >= 0) {
                emit_byte(as, 0xD8 + scode);
                emit_byte(as, 0x98 + dcode);
                return true;
            }
        }
        if (dst->size == SIZE_LONG && src->size == SIZE_LONG) {
            int dcode = get_reg32_code(dst->reg);
            int scode = get_reg32_code(src->reg);
            if (dcode >= 0 && scode >= 0) {
                emit_byte(as, 0xE8 + scode);
                emit_byte(as, 0x98 + dcode);
                return true;
            }
        }
    }

    /* SBC reg, (mem) */
    if (dst->mode == ADDR_REGISTER &&
        (src->mode == ADDR_REGISTER_IND || src->mode == ADDR_INDEXED ||
         src->mode == ADDR_DIRECT)) {
        if (dst->size == SIZE_BYTE) {
            int code = get_reg8_code(dst->reg);
            if (code >= 0) {
                emit_byte(as, 0x80 + (code >> 1));
                emit_mem_operand(as, src);
                emit_byte(as, 0x03 + (code & 1));
                return true;
            }
        } else if (dst->size == SIZE_WORD) {
            int code = get_reg16_code(dst->reg);
            if (code >= 0) {
                emit_byte(as, 0x90);
                emit_mem_operand(as, src);
                emit_byte(as, 0x30 + code);
                return true;
            }
        } else if (dst->size == SIZE_LONG) {
            int code = get_reg32_code(dst->reg);
            if (code >= 0) {
                emit_byte(as, 0xA0);
                emit_mem_operand(as, src);
                emit_byte(as, 0x30 + code);
                return true;
            }
        }
    }

    /* SBC (mem), reg */
    if ((dst->mode == ADDR_REGISTER_IND || dst->mode == ADDR_INDEXED ||
         dst->mode == ADDR_DIRECT) && src->mode == ADDR_REGISTER) {
        if (src->size == SIZE_BYTE) {
            int code = get_reg8_code(src->reg);
            if (code >= 0) {
                emit_byte(as, 0x80 + (code >> 1));
                emit_mem_operand(as, dst);
                emit_byte(as, 0x0B + (code & 1));
                return true;
            }
        } else if (src->size == SIZE_WORD) {
            int code = get_reg16_code(src->reg);
            if (code >= 0) {
                emit_byte(as, 0x90);
                emit_mem_operand(as, dst);
                emit_byte(as, 0x38 + code);
                return true;
            }
        } else if (src->size == SIZE_LONG) {
            int code = get_reg32_code(src->reg);
            if (code >= 0) {
                emit_byte(as, 0xA0);
                emit_mem_operand(as, dst);
                emit_byte(as, 0x38 + code);
                return true;
            }
        }
    }

    error(as, "unsupported SBC operand combination");
    return false;
}

/* CP - Compare */
static bool encode_cp(Assembler *as, Operand *ops, int count) {
    if (count < 2) {
        error(as, "CP requires two operands");
        return false;
    }

    Operand *dst = &ops[0];
    Operand *src = &ops[1];

    /* CP reg, imm */
    if (dst->mode == ADDR_REGISTER && src->mode == ADDR_IMMEDIATE) {
        if (dst->size == SIZE_BYTE) {
            int code = get_reg8_code(dst->reg);
            if (code >= 0) {
                emit_byte(as, 0xC8 + (code >> 1));
                emit_byte(as, 0xF8 + (code & 1));
                emit_byte(as, (uint8_t)src->value);
                return true;
            }
        } else if (dst->size == SIZE_WORD) {
            int code = get_reg16_code(dst->reg);
            if (code >= 0) {
                emit_byte(as, 0xD8 + code);
                emit_byte(as, 0xF8);
                emit_word(as, (uint16_t)src->value);
                return true;
            }
        } else if (dst->size == SIZE_LONG) {
            int code = get_reg32_code(dst->reg);
            if (code >= 0) {
                emit_byte(as, 0xE8 + code);
                emit_byte(as, 0xF8);
                emit_long(as, (uint32_t)src->value);
                return true;
            }
        }
    }

    /* CP reg, reg */
    if (dst->mode == ADDR_REGISTER && src->mode == ADDR_REGISTER) {
        if (dst->size == SIZE_BYTE && src->size == SIZE_BYTE) {
            int dcode = get_reg8_code(dst->reg);
            int scode = get_reg8_code(src->reg);
            if (dcode >= 0 && scode >= 0) {
                emit_byte(as, 0xC8 + (scode >> 1));
                emit_byte(as, 0xB0 + ((scode & 1) << 3) + ((dcode >> 1) << 1) + (dcode & 1));
                return true;
            }
        } else if (dst->size == SIZE_WORD && src->size == SIZE_WORD) {
            int dcode = get_reg16_code(dst->reg);
            int scode = get_reg16_code(src->reg);
            if (dcode >= 0 && scode >= 0) {
                emit_byte(as, 0xD8 + scode);
                emit_byte(as, 0xB0 + dcode);
                return true;
            }
        } else if (dst->size == SIZE_LONG && src->size == SIZE_LONG) {
            int dcode = get_reg32_code(dst->reg);
            int scode = get_reg32_code(src->reg);
            if (dcode >= 0 && scode >= 0) {
                emit_byte(as, 0xE8 + scode);
                emit_byte(as, 0xB0 + dcode);
                return true;
            }
        }
    }

    /* CP reg, (mem) */
    if (dst->mode == ADDR_REGISTER &&
        (src->mode == ADDR_REGISTER_IND || src->mode == ADDR_INDEXED ||
         src->mode == ADDR_DIRECT)) {
        if (dst->size == SIZE_BYTE) {
            int code = get_reg8_code(dst->reg);
            if (code >= 0) {
                emit_byte(as, 0x80 + (code >> 1));
                emit_mem_operand(as, src);
                emit_byte(as, 0x70 + (code & 1));
                return true;
            }
        } else if (dst->size == SIZE_WORD) {
            int code = get_reg16_code(dst->reg);
            if (code >= 0) {
                emit_byte(as, 0x90);
                emit_mem_operand(as, src);
                emit_byte(as, 0x70 + code);
                return true;
            }
        } else if (dst->size == SIZE_LONG) {
            int code = get_reg32_code(dst->reg);
            if (code >= 0) {
                emit_byte(as, 0xA0);
                emit_mem_operand(as, src);
                emit_byte(as, 0x70 + code);
                return true;
            }
        }
    }

    /* CP (mem), reg */
    if ((dst->mode == ADDR_REGISTER_IND || dst->mode == ADDR_INDEXED ||
         dst->mode == ADDR_DIRECT) && src->mode == ADDR_REGISTER) {
        if (src->size == SIZE_BYTE) {
            int code = get_reg8_code(src->reg);
            if (code >= 0) {
                emit_byte(as, 0x80 + (code >> 1));
                emit_mem_operand(as, dst);
                emit_byte(as, 0x78 + (code & 1));
                return true;
            }
        } else if (src->size == SIZE_WORD) {
            int code = get_reg16_code(src->reg);
            if (code >= 0) {
                emit_byte(as, 0x90);
                emit_mem_operand(as, dst);
                emit_byte(as, 0x78 + code);
                return true;
            }
        } else if (src->size == SIZE_LONG) {
            int code = get_reg32_code(src->reg);
            if (code >= 0) {
                emit_byte(as, 0xA0);
                emit_mem_operand(as, dst);
                emit_byte(as, 0x78 + code);
                return true;
            }
        }
    }

    /* CP (mem), imm - byte */
    if ((dst->mode == ADDR_REGISTER_IND || dst->mode == ADDR_INDEXED ||
         dst->mode == ADDR_DIRECT) && src->mode == ADDR_IMMEDIATE) {
        emit_byte(as, 0x80);  /* Byte memory prefix */
        emit_mem_operand(as, dst);
        emit_byte(as, 0x38);  /* CP (mem), imm opcode */
        emit_byte(as, (uint8_t)src->value);
        return true;
    }

    error(as, "unsupported CP operand combination");
    return false;
}

/* CPW - Compare Word (for word-sized memory comparisons) */
static bool encode_cpw(Assembler *as, Operand *ops, int count) {
    if (count < 2) {
        error(as, "CPW requires two operands");
        return false;
    }

    Operand *dst = &ops[0];
    Operand *src = &ops[1];

    /* CPW (mem), imm */
    if ((dst->mode == ADDR_REGISTER_IND || dst->mode == ADDR_INDEXED ||
         dst->mode == ADDR_DIRECT) && src->mode == ADDR_IMMEDIATE) {
        emit_byte(as, 0x90);  /* Word memory prefix */
        emit_mem_operand(as, dst);
        emit_byte(as, 0x38);  /* CPW (mem), imm opcode */
        emit_word(as, (uint16_t)src->value);
        return true;
    }

    error(as, "unsupported CPW operand combination");
    return false;
}

/* INC */
static bool encode_inc(Assembler *as, Operand *ops, int count) {
    if (count < 1) {
        error(as, "INC requires an operand");
        return false;
    }

    int inc_val = 1;
    Operand *target = &ops[0];

    /* Handle both "INC reg" and "INC n, reg/mem" syntax */
    if (count >= 2) {
        if (ops[0].mode == ADDR_IMMEDIATE) {
            /* INC n, target - increment amount first */
            inc_val = (int)ops[0].value;
            target = &ops[1];
        } else if (ops[1].mode == ADDR_IMMEDIATE) {
            /* INC target, n - target first */
            inc_val = (int)ops[1].value;
        }
    }

    if (target->mode == ADDR_REGISTER) {
        if (target->size == SIZE_BYTE) {
            int code = get_reg8_code(target->reg);
            if (code >= 0) {
                emit_byte(as, 0xC8 + (code >> 1));
                emit_byte(as, 0x60 + (code & 1));
                emit_byte(as, (uint8_t)inc_val);
                return true;
            }
        } else if (target->size == SIZE_WORD) {
            int code = get_reg16_code(target->reg);
            if (code >= 0) {
                emit_byte(as, 0xD8 + code);
                emit_byte(as, 0x60);
                emit_byte(as, (uint8_t)inc_val);
                return true;
            }
        } else if (target->size == SIZE_LONG) {
            int code = get_reg32_code(target->reg);
            if (code >= 0) {
                emit_byte(as, 0xE8 + code);
                emit_byte(as, 0x60);
                emit_byte(as, (uint8_t)inc_val);
                return true;
            }
        }
    }

    /* INC (mem) */
    if (target->mode == ADDR_REGISTER_IND || target->mode == ADDR_INDEXED ||
        target->mode == ADDR_DIRECT) {
        emit_byte(as, 0x80);
        emit_mem_operand(as, target);
        emit_byte(as, 0x60);
        emit_byte(as, (uint8_t)inc_val);
        return true;
    }

    error(as, "unsupported INC operand");
    return false;
}

/* DEC */
static bool encode_dec(Assembler *as, Operand *ops, int count) {
    if (count < 1) {
        error(as, "DEC requires an operand");
        return false;
    }

    int dec_val = 1;
    Operand *target = &ops[0];

    /* Handle both "DEC reg" and "DEC n, reg/mem" syntax */
    if (count >= 2) {
        if (ops[0].mode == ADDR_IMMEDIATE) {
            /* DEC n, target - decrement amount first */
            dec_val = (int)ops[0].value;
            target = &ops[1];
        } else if (ops[1].mode == ADDR_IMMEDIATE) {
            /* DEC target, n - target first */
            dec_val = (int)ops[1].value;
        }
    }

    if (target->mode == ADDR_REGISTER) {
        if (target->size == SIZE_BYTE) {
            int code = get_reg8_code(target->reg);
            if (code >= 0) {
                emit_byte(as, 0xC8 + (code >> 1));
                emit_byte(as, 0x68 + (code & 1));
                emit_byte(as, (uint8_t)dec_val);
                return true;
            }
        } else if (target->size == SIZE_WORD) {
            int code = get_reg16_code(target->reg);
            if (code >= 0) {
                emit_byte(as, 0xD8 + code);
                emit_byte(as, 0x68);
                emit_byte(as, (uint8_t)dec_val);
                return true;
            }
        } else if (target->size == SIZE_LONG) {
            int code = get_reg32_code(target->reg);
            if (code >= 0) {
                emit_byte(as, 0xE8 + code);
                emit_byte(as, 0x68);
                emit_byte(as, (uint8_t)dec_val);
                return true;
            }
        }
    }

    /* DEC (mem) */
    if (target->mode == ADDR_REGISTER_IND || target->mode == ADDR_INDEXED ||
        target->mode == ADDR_DIRECT) {
        emit_byte(as, 0x80);
        emit_mem_operand(as, target);
        emit_byte(as, 0x68);
        emit_byte(as, (uint8_t)dec_val);
        return true;
    }

    error(as, "unsupported DEC operand");
    return false;
}

/* INCW - Increment Word (memory) */
static bool encode_incw(Assembler *as, Operand *ops, int count) {
    if (count < 1) {
        error(as, "INCW requires an operand");
        return false;
    }

    int inc_val = 1;
    Operand *target = &ops[0];

    /* Handle "INCW n, (mem)" syntax */
    if (count >= 2) {
        if (ops[0].mode == ADDR_IMMEDIATE) {
            inc_val = (int)ops[0].value;
            target = &ops[1];
        }
    }

    /* INCW (mem) */
    if (target->mode == ADDR_REGISTER_IND || target->mode == ADDR_INDEXED ||
        target->mode == ADDR_DIRECT) {
        emit_byte(as, 0x90);  /* Word memory prefix */
        emit_mem_operand(as, target);
        emit_byte(as, 0x60);
        emit_byte(as, (uint8_t)inc_val);
        return true;
    }

    error(as, "unsupported INCW operand");
    return false;
}

/* DECW - Decrement Word (memory) */
static bool encode_decw(Assembler *as, Operand *ops, int count) {
    if (count < 1) {
        error(as, "DECW requires an operand");
        return false;
    }

    int dec_val = 1;
    Operand *target = &ops[0];

    /* Handle "DECW n, (mem)" syntax */
    if (count >= 2) {
        if (ops[0].mode == ADDR_IMMEDIATE) {
            dec_val = (int)ops[0].value;
            target = &ops[1];
        }
    }

    /* DECW (mem) */
    if (target->mode == ADDR_REGISTER_IND || target->mode == ADDR_INDEXED ||
        target->mode == ADDR_DIRECT) {
        emit_byte(as, 0x90);  /* Word memory prefix */
        emit_mem_operand(as, target);
        emit_byte(as, 0x68);
        emit_byte(as, (uint8_t)dec_val);
        return true;
    }

    error(as, "unsupported DECW operand");
    return false;
}

/* NEG - Negate */
static bool encode_neg(Assembler *as, Operand *ops, int count) {
    if (count < 1) {
        error(as, "NEG requires an operand");
        return false;
    }

    if (ops[0].mode == ADDR_REGISTER) {
        if (ops[0].size == SIZE_BYTE) {
            int code = get_reg8_code(ops[0].reg);
            if (code >= 0) {
                emit_byte(as, 0xC8 + (code >> 1));
                emit_byte(as, 0x04 + (code & 1));
                return true;
            }
        } else if (ops[0].size == SIZE_WORD) {
            int code = get_reg16_code(ops[0].reg);
            if (code >= 0) {
                emit_byte(as, 0xD8 + code);
                emit_byte(as, 0x04);
                return true;
            }
        } else if (ops[0].size == SIZE_LONG) {
            int code = get_reg32_code(ops[0].reg);
            if (code >= 0) {
                emit_byte(as, 0xE8 + code);
                emit_byte(as, 0x04);
                return true;
            }
        }
    }

    error(as, "unsupported NEG operand");
    return false;
}

/* MUL - Unsigned multiply */
static bool encode_mul(Assembler *as, Operand *ops, int count) {
    if (count < 2) {
        error(as, "MUL requires two operands");
        return false;
    }

    Operand *dst = &ops[0];
    Operand *src = &ops[1];

    /* MUL reg, imm */
    if (dst->mode == ADDR_REGISTER && src->mode == ADDR_IMMEDIATE) {
        if (dst->size == SIZE_BYTE) {
            int code = get_reg8_code(dst->reg);
            if (code >= 0) {
                emit_byte(as, 0xC8 + (code >> 1) + ((code & 1) ? 1 : 0));
                emit_byte(as, 0x08);
                emit_byte(as, (uint8_t)src->value);
                return true;
            }
        } else if (dst->size == SIZE_WORD) {
            int code = get_reg16_code(dst->reg);
            if (code >= 0) {
                emit_byte(as, 0xD8 + code);
                emit_byte(as, 0x08);
                emit_word(as, (uint16_t)src->value);
                return true;
            }
        }
    }

    /* MUL reg, reg */
    if (dst->mode == ADDR_REGISTER && src->mode == ADDR_REGISTER) {
        /* MUL RR, r - word register x byte register -> long result */
        if (dst->size == SIZE_WORD && src->size == SIZE_BYTE) {
            int dcode = get_reg16_code(dst->reg);
            int scode = get_reg8_code(src->reg);
            if (dcode >= 0 && scode >= 0) {
                emit_byte(as, 0xC8 + (scode >> 1));
                emit_byte(as, 0x40 + ((scode & 1) << 3) + dcode);
                return true;
            }
        }
        if (dst->size == SIZE_WORD && src->size == SIZE_WORD) {
            int dcode = get_reg16_code(dst->reg);
            int scode = get_reg16_code(src->reg);
            if (dcode >= 0 && scode >= 0) {
                emit_byte(as, 0xD8 + scode);
                emit_byte(as, 0x40 + dcode);
                return true;
            }
        }
        /* MUL XRR, RR - long register x word register -> qword result */
        if (dst->size == SIZE_LONG && src->size == SIZE_WORD) {
            int dcode = get_reg32_code(dst->reg);
            int scode = get_reg16_code(src->reg);
            if (dcode >= 0 && scode >= 0) {
                emit_byte(as, 0xD8 + scode);
                emit_byte(as, 0x48 + dcode);
                return true;
            }
        }
    }

    error(as, "unsupported MUL operand combination");
    return false;
}

/* MULS - Signed multiply */
static bool encode_muls(Assembler *as, Operand *ops, int count) {
    if (count < 2) {
        error(as, "MULS requires two operands");
        return false;
    }

    Operand *dst = &ops[0];
    Operand *src = &ops[1];

    /* MULS reg, imm */
    if (dst->mode == ADDR_REGISTER && src->mode == ADDR_IMMEDIATE) {
        if (dst->size == SIZE_BYTE) {
            int code = get_reg8_code(dst->reg);
            if (code >= 0) {
                emit_byte(as, 0xC8 + (code >> 1) + ((code & 1) ? 1 : 0));
                emit_byte(as, 0x09);
                emit_byte(as, (uint8_t)src->value);
                return true;
            }
        } else if (dst->size == SIZE_WORD) {
            int code = get_reg16_code(dst->reg);
            if (code >= 0) {
                emit_byte(as, 0xD8 + code);
                emit_byte(as, 0x09);
                emit_word(as, (uint16_t)src->value);
                return true;
            }
        }
    }

    /* MULS reg, reg */
    if (dst->mode == ADDR_REGISTER && src->mode == ADDR_REGISTER) {
        if (dst->size == SIZE_WORD && src->size == SIZE_WORD) {
            int dcode = get_reg16_code(dst->reg);
            int scode = get_reg16_code(src->reg);
            if (dcode >= 0 && scode >= 0) {
                emit_byte(as, 0xD8 + scode);
                emit_byte(as, 0x48 + dcode);
                return true;
            }
        } else if (dst->size == SIZE_LONG && src->size == SIZE_WORD) {
            /* MULS XWA, rr */
            int dcode = get_reg32_code(dst->reg);
            int scode = get_reg16_code(src->reg);
            if (dcode >= 0 && scode >= 0) {
                emit_byte(as, 0xD8 + scode);
                emit_byte(as, 0x48 + dcode);
                return true;
            }
        }
    }

    error(as, "unsupported MULS operand combination");
    return false;
}

/* DIV - Unsigned divide */
static bool encode_div(Assembler *as, Operand *ops, int count) {
    if (count < 2) {
        error(as, "DIV requires two operands");
        return false;
    }

    Operand *dst = &ops[0];
    Operand *src = &ops[1];

    /* DIV reg, imm */
    if (dst->mode == ADDR_REGISTER && src->mode == ADDR_IMMEDIATE) {
        if (dst->size == SIZE_BYTE) {
            int code = get_reg8_code(dst->reg);
            if (code >= 0) {
                emit_byte(as, 0xC8 + (code >> 1) + ((code & 1) ? 1 : 0));
                emit_byte(as, 0x0A);
                emit_byte(as, (uint8_t)src->value);
                return true;
            }
        } else if (dst->size == SIZE_WORD) {
            int code = get_reg16_code(dst->reg);
            if (code >= 0) {
                emit_byte(as, 0xD8 + code);
                emit_byte(as, 0x0A);
                emit_word(as, (uint16_t)src->value);
                return true;
            }
        }
    }

    /* DIV reg, reg */
    if (dst->mode == ADDR_REGISTER && src->mode == ADDR_REGISTER) {
        if (dst->size == SIZE_WORD && src->size == SIZE_WORD) {
            int dcode = get_reg16_code(dst->reg);
            int scode = get_reg16_code(src->reg);
            if (dcode >= 0 && scode >= 0) {
                emit_byte(as, 0xD8 + scode);
                emit_byte(as, 0x50 + dcode);
                return true;
            }
        }
        /* DIV XRR, RR - 32-bit / 16-bit */
        if (dst->size == SIZE_LONG && src->size == SIZE_WORD) {
            int dcode = get_reg32_code(dst->reg);
            int scode = get_reg16_code(src->reg);
            if (dcode >= 0 && scode >= 0) {
                emit_byte(as, 0xD8 + scode);
                emit_byte(as, 0x58 + dcode);
                return true;
            }
        }
    }

    error(as, "unsupported DIV operand combination");
    return false;
}

/* DIVS - Signed divide */
static bool encode_divs(Assembler *as, Operand *ops, int count) {
    if (count < 2) {
        error(as, "DIVS requires two operands");
        return false;
    }

    Operand *dst = &ops[0];
    Operand *src = &ops[1];

    /* DIVS reg, imm */
    if (dst->mode == ADDR_REGISTER && src->mode == ADDR_IMMEDIATE) {
        if (dst->size == SIZE_WORD) {
            int code = get_reg16_code(dst->reg);
            if (code >= 0) {
                emit_byte(as, 0xD8 + code);
                emit_byte(as, 0x0B);
                emit_word(as, (uint16_t)src->value);
                return true;
            }
        }
    }

    /* DIVS reg, reg */
    if (dst->mode == ADDR_REGISTER && src->mode == ADDR_REGISTER) {
        if (dst->size == SIZE_WORD && src->size == SIZE_WORD) {
            int dcode = get_reg16_code(dst->reg);
            int scode = get_reg16_code(src->reg);
            if (dcode >= 0 && scode >= 0) {
                emit_byte(as, 0xD8 + scode);
                emit_byte(as, 0x58 + dcode);
                return true;
            }
        }
        /* DIVS XRR, RR - 32-bit / 16-bit signed */
        if (dst->size == SIZE_LONG && src->size == SIZE_WORD) {
            int dcode = get_reg32_code(dst->reg);
            int scode = get_reg16_code(src->reg);
            if (dcode >= 0 && scode >= 0) {
                emit_byte(as, 0xD8 + scode);
                emit_byte(as, 0x5C + dcode);
                return true;
            }
        }
    }

    error(as, "unsupported DIVS operand combination");
    return false;
}

/* DAA - Decimal adjust after addition */
static bool encode_daa(Assembler *as, Operand *ops, int count) {
    if (count < 1) {
        error(as, "DAA requires a register");
        return false;
    }
    if (ops[0].mode != ADDR_REGISTER || ops[0].size != SIZE_BYTE) {
        error(as, "DAA requires 8-bit register");
        return false;
    }
    int code = get_reg8_code(ops[0].reg);
    if (code < 0) {
        error(as, "invalid DAA register");
        return false;
    }
    emit_byte(as, 0xC8 + (code >> 1));
    emit_byte(as, 0x10 + (code & 1));
    return true;
}

/* ============== Logical Instructions ============== */

/* AND */
static bool encode_and(Assembler *as, Operand *ops, int count) {
    if (count < 2) {
        error(as, "AND requires two operands");
        return false;
    }

    Operand *dst = &ops[0];
    Operand *src = &ops[1];

    if (dst->mode == ADDR_REGISTER && src->mode == ADDR_IMMEDIATE) {
        if (dst->size == SIZE_BYTE) {
            int code = get_reg8_code(dst->reg);
            if (code >= 0) {
                emit_byte(as, 0xC8 + (code >> 1));
                emit_byte(as, 0xCC + (code & 1));
                emit_byte(as, (uint8_t)src->value);
                return true;
            }
        } else if (dst->size == SIZE_WORD) {
            int code = get_reg16_code(dst->reg);
            if (code >= 0) {
                emit_byte(as, 0xD8 + code);
                emit_byte(as, 0xCC);
                emit_word(as, (uint16_t)src->value);
                return true;
            }
        } else if (dst->size == SIZE_LONG) {
            int code = get_reg32_code(dst->reg);
            if (code >= 0) {
                emit_byte(as, 0xE8 + code);
                emit_byte(as, 0xCC);
                emit_long(as, (uint32_t)src->value);
                return true;
            }
        }
    }

    /* AND reg, reg */
    if (dst->mode == ADDR_REGISTER && src->mode == ADDR_REGISTER) {
        if (dst->size == SIZE_BYTE && src->size == SIZE_BYTE) {
            int dcode = get_reg8_code(dst->reg);
            int scode = get_reg8_code(src->reg);
            if (dcode >= 0 && scode >= 0) {
                emit_byte(as, 0xC8 + (scode >> 1));
                emit_byte(as, 0xA0 + ((scode & 1) << 3) + ((dcode >> 1) << 1) + (dcode & 1));
                return true;
            }
        } else if (dst->size == SIZE_WORD && src->size == SIZE_WORD) {
            int dcode = get_reg16_code(dst->reg);
            int scode = get_reg16_code(src->reg);
            if (dcode >= 0 && scode >= 0) {
                emit_byte(as, 0xD8 + scode);
                emit_byte(as, 0xC0 + dcode);
                return true;
            }
        } else if (dst->size == SIZE_LONG && src->size == SIZE_LONG) {
            int dcode = get_reg32_code(dst->reg);
            int scode = get_reg32_code(src->reg);
            if (dcode >= 0 && scode >= 0) {
                emit_byte(as, 0xE8 + scode);
                emit_byte(as, 0xC0 + dcode);
                return true;
            }
        }
    }

    /* AND (mem), imm */
    if ((dst->mode == ADDR_DIRECT || dst->mode == ADDR_REGISTER_IND ||
         dst->mode == ADDR_INDEXED) && src->mode == ADDR_IMMEDIATE) {
        emit_byte(as, 0xB0);
        emit_mem_operand(as, dst);
        emit_byte(as, 0x2C);
        emit_byte(as, (uint8_t)src->value);
        return true;
    }

    /* AND reg, (mem) */
    if (dst->mode == ADDR_REGISTER &&
        (src->mode == ADDR_REGISTER_IND || src->mode == ADDR_INDEXED ||
         src->mode == ADDR_DIRECT)) {
        if (dst->size == SIZE_BYTE) {
            int code = get_reg8_code(dst->reg);
            if (code >= 0) {
                emit_byte(as, 0x80 + (code >> 1));
                emit_mem_operand(as, src);
                emit_byte(as, 0x04 + (code & 1));
                return true;
            }
        } else if (dst->size == SIZE_WORD) {
            int code = get_reg16_code(dst->reg);
            if (code >= 0) {
                emit_byte(as, 0x90);
                emit_mem_operand(as, src);
                emit_byte(as, 0x40 + code);
                return true;
            }
        } else if (dst->size == SIZE_LONG) {
            int code = get_reg32_code(dst->reg);
            if (code >= 0) {
                emit_byte(as, 0xA0);
                emit_mem_operand(as, src);
                emit_byte(as, 0x40 + code);
                return true;
            }
        }
    }

    /* AND (mem), reg */
    if ((dst->mode == ADDR_REGISTER_IND || dst->mode == ADDR_INDEXED ||
         dst->mode == ADDR_DIRECT) && src->mode == ADDR_REGISTER) {
        if (src->size == SIZE_BYTE) {
            int code = get_reg8_code(src->reg);
            if (code >= 0) {
                emit_byte(as, 0x80 + (code >> 1));
                emit_mem_operand(as, dst);
                emit_byte(as, 0x0C + (code & 1));
                return true;
            }
        } else if (src->size == SIZE_WORD) {
            int code = get_reg16_code(src->reg);
            if (code >= 0) {
                emit_byte(as, 0x90);
                emit_mem_operand(as, dst);
                emit_byte(as, 0x48 + code);
                return true;
            }
        } else if (src->size == SIZE_LONG) {
            int code = get_reg32_code(src->reg);
            if (code >= 0) {
                emit_byte(as, 0xA0);
                emit_mem_operand(as, dst);
                emit_byte(as, 0x48 + code);
                return true;
            }
        }
    }

    error(as, "unsupported AND operand combination");
    return false;
}

/* OR */
static bool encode_or(Assembler *as, Operand *ops, int count) {
    if (count < 2) {
        error(as, "OR requires two operands");
        return false;
    }

    Operand *dst = &ops[0];
    Operand *src = &ops[1];

    if (dst->mode == ADDR_REGISTER && src->mode == ADDR_IMMEDIATE) {
        if (dst->size == SIZE_BYTE) {
            int code = get_reg8_code(dst->reg);
            if (code >= 0) {
                emit_byte(as, 0xC8 + (code >> 1));
                emit_byte(as, 0xCE + (code & 1));
                emit_byte(as, (uint8_t)src->value);
                return true;
            }
        } else if (dst->size == SIZE_WORD) {
            int code = get_reg16_code(dst->reg);
            if (code >= 0) {
                emit_byte(as, 0xD8 + code);
                emit_byte(as, 0xCE);
                emit_word(as, (uint16_t)src->value);
                return true;
            }
        } else if (dst->size == SIZE_LONG) {
            int code = get_reg32_code(dst->reg);
            if (code >= 0) {
                emit_byte(as, 0xE8 + code);
                emit_byte(as, 0xCE);
                emit_long(as, (uint32_t)src->value);
                return true;
            }
        }
    }

    /* OR reg, reg */
    if (dst->mode == ADDR_REGISTER && src->mode == ADDR_REGISTER) {
        if (dst->size == SIZE_BYTE && src->size == SIZE_BYTE) {
            int dcode = get_reg8_code(dst->reg);
            int scode = get_reg8_code(src->reg);
            if (dcode >= 0 && scode >= 0) {
                emit_byte(as, 0xC8 + (scode >> 1));
                emit_byte(as, 0xA8 + ((scode & 1) << 3) + ((dcode >> 1) << 1) + (dcode & 1));
                return true;
            }
        } else if (dst->size == SIZE_WORD && src->size == SIZE_WORD) {
            int dcode = get_reg16_code(dst->reg);
            int scode = get_reg16_code(src->reg);
            if (dcode >= 0 && scode >= 0) {
                emit_byte(as, 0xD8 + scode);
                emit_byte(as, 0xC8 + dcode);
                return true;
            }
        } else if (dst->size == SIZE_LONG && src->size == SIZE_LONG) {
            int dcode = get_reg32_code(dst->reg);
            int scode = get_reg32_code(src->reg);
            if (dcode >= 0 && scode >= 0) {
                emit_byte(as, 0xE8 + scode);
                emit_byte(as, 0xC8 + dcode);
                return true;
            }
        }
    }

    /* OR (mem), imm */
    if ((dst->mode == ADDR_DIRECT || dst->mode == ADDR_REGISTER_IND ||
         dst->mode == ADDR_INDEXED) && src->mode == ADDR_IMMEDIATE) {
        emit_byte(as, 0xB0);
        emit_mem_operand(as, dst);
        emit_byte(as, 0x2E);
        emit_byte(as, (uint8_t)src->value);
        return true;
    }

    /* OR reg, (mem) */
    if (dst->mode == ADDR_REGISTER &&
        (src->mode == ADDR_REGISTER_IND || src->mode == ADDR_INDEXED ||
         src->mode == ADDR_DIRECT)) {
        if (dst->size == SIZE_BYTE) {
            int code = get_reg8_code(dst->reg);
            if (code >= 0) {
                emit_byte(as, 0x80 + (code >> 1));
                emit_mem_operand(as, src);
                emit_byte(as, 0x06 + (code & 1));
                return true;
            }
        } else if (dst->size == SIZE_WORD) {
            int code = get_reg16_code(dst->reg);
            if (code >= 0) {
                emit_byte(as, 0x90);
                emit_mem_operand(as, src);
                emit_byte(as, 0x60 + code);
                return true;
            }
        } else if (dst->size == SIZE_LONG) {
            int code = get_reg32_code(dst->reg);
            if (code >= 0) {
                emit_byte(as, 0xA0);
                emit_mem_operand(as, src);
                emit_byte(as, 0x60 + code);
                return true;
            }
        }
    }

    /* OR (mem), reg */
    if ((dst->mode == ADDR_REGISTER_IND || dst->mode == ADDR_INDEXED ||
         dst->mode == ADDR_DIRECT) && src->mode == ADDR_REGISTER) {
        if (src->size == SIZE_BYTE) {
            int code = get_reg8_code(src->reg);
            if (code >= 0) {
                emit_byte(as, 0x80 + (code >> 1));
                emit_mem_operand(as, dst);
                emit_byte(as, 0x0E + (code & 1));
                return true;
            }
        } else if (src->size == SIZE_WORD) {
            int code = get_reg16_code(src->reg);
            if (code >= 0) {
                emit_byte(as, 0x90);
                emit_mem_operand(as, dst);
                emit_byte(as, 0x68 + code);
                return true;
            }
        } else if (src->size == SIZE_LONG) {
            int code = get_reg32_code(src->reg);
            if (code >= 0) {
                emit_byte(as, 0xA0);
                emit_mem_operand(as, dst);
                emit_byte(as, 0x68 + code);
                return true;
            }
        }
    }

    error(as, "unsupported OR operand combination");
    return false;
}

/* ORW - OR Word (memory) */
static bool encode_orw(Assembler *as, Operand *ops, int count) {
    if (count < 2) {
        error(as, "ORW requires two operands");
        return false;
    }

    Operand *dst = &ops[0];
    Operand *src = &ops[1];

    /* ORW (mem), imm16 */
    if ((dst->mode == ADDR_DIRECT || dst->mode == ADDR_REGISTER_IND ||
         dst->mode == ADDR_INDEXED) && src->mode == ADDR_IMMEDIATE) {
        emit_byte(as, 0x90);  /* Word memory prefix */
        emit_mem_operand(as, dst);
        emit_byte(as, 0x2C);  /* OR opcode */
        emit_word(as, (uint16_t)src->value);
        return true;
    }

    error(as, "unsupported ORW operand combination");
    return false;
}

/* ANDW - AND Word (memory) */
static bool encode_andw(Assembler *as, Operand *ops, int count) {
    if (count < 2) {
        error(as, "ANDW requires two operands");
        return false;
    }

    Operand *dst = &ops[0];
    Operand *src = &ops[1];

    /* ANDW (mem), imm16 */
    if ((dst->mode == ADDR_DIRECT || dst->mode == ADDR_REGISTER_IND ||
         dst->mode == ADDR_INDEXED) && src->mode == ADDR_IMMEDIATE) {
        emit_byte(as, 0x90);  /* Word memory prefix */
        emit_mem_operand(as, dst);
        emit_byte(as, 0x24);  /* AND opcode */
        emit_word(as, (uint16_t)src->value);
        return true;
    }

    error(as, "unsupported ANDW operand combination");
    return false;
}

/* ADDW - ADD Word (memory) */
static bool encode_addw(Assembler *as, Operand *ops, int count) {
    if (count < 2) {
        error(as, "ADDW requires two operands");
        return false;
    }

    Operand *dst = &ops[0];
    Operand *src = &ops[1];

    /* ADDW (mem), imm16 */
    if ((dst->mode == ADDR_DIRECT || dst->mode == ADDR_REGISTER_IND ||
         dst->mode == ADDR_INDEXED) && src->mode == ADDR_IMMEDIATE) {
        emit_byte(as, 0x90);  /* Word memory prefix */
        emit_mem_operand(as, dst);
        emit_byte(as, 0x08);  /* ADD opcode */
        emit_word(as, (uint16_t)src->value);
        return true;
    }

    error(as, "unsupported ADDW operand combination");
    return false;
}

/* XOR */
static bool encode_xor(Assembler *as, Operand *ops, int count) {
    if (count < 2) {
        error(as, "XOR requires two operands");
        return false;
    }

    Operand *dst = &ops[0];
    Operand *src = &ops[1];

    if (dst->mode == ADDR_REGISTER && src->mode == ADDR_IMMEDIATE) {
        if (dst->size == SIZE_BYTE) {
            int code = get_reg8_code(dst->reg);
            if (code >= 0) {
                emit_byte(as, 0xC8 + (code >> 1));
                emit_byte(as, 0xD0 + (code & 1));
                emit_byte(as, (uint8_t)src->value);
                return true;
            }
        } else if (dst->size == SIZE_WORD) {
            int code = get_reg16_code(dst->reg);
            if (code >= 0) {
                emit_byte(as, 0xD8 + code);
                emit_byte(as, 0xD0);
                emit_word(as, (uint16_t)src->value);
                return true;
            }
        } else if (dst->size == SIZE_LONG) {
            int code = get_reg32_code(dst->reg);
            if (code >= 0) {
                emit_byte(as, 0xE8 + code);
                emit_byte(as, 0xD0);
                emit_long(as, (uint32_t)src->value);
                return true;
            }
        }
    }

    /* XOR reg, reg */
    if (dst->mode == ADDR_REGISTER && src->mode == ADDR_REGISTER) {
        if (dst->size == SIZE_BYTE && src->size == SIZE_BYTE) {
            int dcode = get_reg8_code(dst->reg);
            int scode = get_reg8_code(src->reg);
            if (dcode >= 0 && scode >= 0) {
                emit_byte(as, 0xC8 + (scode >> 1));
                emit_byte(as, 0xB8 + ((scode & 1) << 3) + ((dcode >> 1) << 1) + (dcode & 1));
                return true;
            }
        } else if (dst->size == SIZE_WORD && src->size == SIZE_WORD) {
            int dcode = get_reg16_code(dst->reg);
            int scode = get_reg16_code(src->reg);
            if (dcode >= 0 && scode >= 0) {
                emit_byte(as, 0xD8 + scode);
                emit_byte(as, 0xD0 + dcode);
                return true;
            }
        } else if (dst->size == SIZE_LONG && src->size == SIZE_LONG) {
            int dcode = get_reg32_code(dst->reg);
            int scode = get_reg32_code(src->reg);
            if (dcode >= 0 && scode >= 0) {
                emit_byte(as, 0xE8 + scode);
                emit_byte(as, 0xD0 + dcode);
                return true;
            }
        }
    }

    /* XOR (mem), imm */
    if ((dst->mode == ADDR_DIRECT || dst->mode == ADDR_REGISTER_IND ||
         dst->mode == ADDR_INDEXED) && src->mode == ADDR_IMMEDIATE) {
        emit_byte(as, 0xB0);
        emit_mem_operand(as, dst);
        emit_byte(as, 0x30);
        emit_byte(as, (uint8_t)src->value);
        return true;
    }

    /* XOR reg, (mem) */
    if (dst->mode == ADDR_REGISTER &&
        (src->mode == ADDR_REGISTER_IND || src->mode == ADDR_INDEXED ||
         src->mode == ADDR_DIRECT)) {
        if (dst->size == SIZE_BYTE) {
            int code = get_reg8_code(dst->reg);
            if (code >= 0) {
                emit_byte(as, 0x80 + (code >> 1));
                emit_mem_operand(as, src);
                emit_byte(as, 0x10 + (code & 1));
                return true;
            }
        } else if (dst->size == SIZE_WORD) {
            int code = get_reg16_code(dst->reg);
            if (code >= 0) {
                emit_byte(as, 0x90);
                emit_mem_operand(as, src);
                emit_byte(as, 0x80 + code);
                return true;
            }
        } else if (dst->size == SIZE_LONG) {
            int code = get_reg32_code(dst->reg);
            if (code >= 0) {
                emit_byte(as, 0xA0);
                emit_mem_operand(as, src);
                emit_byte(as, 0x80 + code);
                return true;
            }
        }
    }

    /* XOR (mem), reg */
    if ((dst->mode == ADDR_REGISTER_IND || dst->mode == ADDR_INDEXED ||
         dst->mode == ADDR_DIRECT) && src->mode == ADDR_REGISTER) {
        if (src->size == SIZE_BYTE) {
            int code = get_reg8_code(src->reg);
            if (code >= 0) {
                emit_byte(as, 0x80 + (code >> 1));
                emit_mem_operand(as, dst);
                emit_byte(as, 0x18 + (code & 1));
                return true;
            }
        } else if (src->size == SIZE_WORD) {
            int code = get_reg16_code(src->reg);
            if (code >= 0) {
                emit_byte(as, 0x90);
                emit_mem_operand(as, dst);
                emit_byte(as, 0x88 + code);
                return true;
            }
        } else if (src->size == SIZE_LONG) {
            int code = get_reg32_code(src->reg);
            if (code >= 0) {
                emit_byte(as, 0xA0);
                emit_mem_operand(as, dst);
                emit_byte(as, 0x88 + code);
                return true;
            }
        }
    }

    error(as, "unsupported XOR operand combination");
    return false;
}

/* CPL - Complement */
static bool encode_cpl(Assembler *as, Operand *ops, int count) {
    if (count < 1) {
        error(as, "CPL requires an operand");
        return false;
    }

    if (ops[0].mode == ADDR_REGISTER) {
        if (ops[0].size == SIZE_BYTE) {
            int code = get_reg8_code(ops[0].reg);
            if (code >= 0) {
                emit_byte(as, 0xC8 + (code >> 1));
                emit_byte(as, 0x06 + (code & 1));
                return true;
            }
        } else if (ops[0].size == SIZE_WORD) {
            int code = get_reg16_code(ops[0].reg);
            if (code >= 0) {
                emit_byte(as, 0xD8 + code);
                emit_byte(as, 0x06);
                return true;
            }
        } else if (ops[0].size == SIZE_LONG) {
            int code = get_reg32_code(ops[0].reg);
            if (code >= 0) {
                emit_byte(as, 0xE8 + code);
                emit_byte(as, 0x06);
                return true;
            }
        }
    }

    error(as, "unsupported CPL operand");
    return false;
}

/* ============== Shift/Rotate Instructions ============== */

/* Generic shift/rotate encoder */
static bool encode_shift(Assembler *as, Operand *ops, int count, uint8_t opcode) {
    if (count < 1) {
        error(as, "shift/rotate requires an operand");
        return false;
    }

    int amount = 1;
    Operand *reg_op = &ops[0];

    /* Check for amount,reg form */
    if (count >= 2 && ops[0].mode == ADDR_IMMEDIATE) {
        amount = (int)ops[0].value;
        reg_op = &ops[1];
    }

    if (reg_op->mode == ADDR_REGISTER) {
        if (reg_op->size == SIZE_BYTE) {
            int code = get_reg8_code(reg_op->reg);
            if (code >= 0) {
                emit_byte(as, 0xC8 + (code >> 1));
                emit_byte(as, opcode + (code & 1));
                emit_byte(as, (uint8_t)amount);
                return true;
            }
        } else if (reg_op->size == SIZE_WORD) {
            int code = get_reg16_code(reg_op->reg);
            if (code >= 0) {
                emit_byte(as, 0xD8 + code);
                emit_byte(as, opcode);
                emit_byte(as, (uint8_t)amount);
                return true;
            }
        } else if (reg_op->size == SIZE_LONG) {
            int code = get_reg32_code(reg_op->reg);
            if (code >= 0) {
                emit_byte(as, 0xE8 + code);
                emit_byte(as, opcode);
                emit_byte(as, (uint8_t)amount);
                return true;
            }
        }
    }

    error(as, "unsupported shift/rotate operand");
    return false;
}

/* RLC - Rotate left through carry */
static bool encode_rlc(Assembler *as, Operand *ops, int count) {
    return encode_shift(as, ops, count, 0xE8);
}

/* RRC - Rotate right through carry */
static bool encode_rrc(Assembler *as, Operand *ops, int count) {
    return encode_shift(as, ops, count, 0xE9);
}

/* RL - Rotate left */
static bool encode_rl(Assembler *as, Operand *ops, int count) {
    return encode_shift(as, ops, count, 0xEA);
}

/* RR - Rotate right */
static bool encode_rr(Assembler *as, Operand *ops, int count) {
    return encode_shift(as, ops, count, 0xEB);
}

/* SLA - Shift left arithmetic */
static bool encode_sla(Assembler *as, Operand *ops, int count) {
    return encode_shift(as, ops, count, 0xEC);
}

/* SRA - Shift right arithmetic */
static bool encode_sra(Assembler *as, Operand *ops, int count) {
    return encode_shift(as, ops, count, 0xED);
}

/* SLL - Shift left logical */
static bool encode_sll(Assembler *as, Operand *ops, int count) {
    return encode_shift(as, ops, count, 0xEE);
}

/* SRL - Shift right logical */
static bool encode_srl(Assembler *as, Operand *ops, int count) {
    return encode_shift(as, ops, count, 0xEF);
}

/* ============== Bit Instructions ============== */

/* BIT - Test bit */
static bool encode_bit(Assembler *as, Operand *ops, int count) {
    if (count < 2) {
        error(as, "BIT requires bit number and operand");
        return false;
    }

    if (ops[1].mode == ADDR_REGISTER) {
        if (ops[1].size == SIZE_BYTE) {
            int bit = (int)ops[0].value & 7;
            int code = get_reg8_code(ops[1].reg);
            if (code >= 0) {
                emit_byte(as, 0xC8 + (code >> 1));
                emit_byte(as, 0x58 + (code & 1));
                emit_byte(as, bit);
                return true;
            }
        }
        if (ops[1].size == SIZE_WORD) {
            int bit = (int)ops[0].value & 15;
            int code = get_reg16_code(ops[1].reg);
            if (code >= 0) {
                emit_byte(as, 0xE8 + code);
                emit_byte(as, 0x18);
                emit_byte(as, bit);
                return true;
            }
        }
    }

    /* BIT n, (mem) */
    if (ops[1].mode == ADDR_DIRECT || ops[1].mode == ADDR_REGISTER_IND ||
        ops[1].mode == ADDR_INDEXED) {
        int bit = (int)ops[0].value & 7;
        emit_byte(as, 0xB0);
        emit_mem_operand(as, &ops[1]);
        emit_byte(as, 0xC0 + bit);
        return true;
    }

    error(as, "unsupported BIT operand");
    return false;
}

/* SET - Set bit */
static bool encode_set(Assembler *as, Operand *ops, int count) {
    if (count < 2) {
        error(as, "SET requires bit number and operand");
        return false;
    }

    if (ops[1].mode == ADDR_REGISTER) {
        if (ops[1].size == SIZE_BYTE) {
            int bit = (int)ops[0].value & 7;
            int code = get_reg8_code(ops[1].reg);
            if (code >= 0) {
                emit_byte(as, 0xC8 + (code >> 1));
                emit_byte(as, 0x70 + (code & 1));
                emit_byte(as, bit);
                return true;
            }
        }
        if (ops[1].size == SIZE_WORD) {
            int bit = (int)ops[0].value & 15;
            int code = get_reg16_code(ops[1].reg);
            if (code >= 0) {
                emit_byte(as, 0xE8 + code);
                emit_byte(as, 0x30);
                emit_byte(as, bit);
                return true;
            }
        }
    }

    /* SET n, (mem) */
    if (ops[1].mode == ADDR_DIRECT || ops[1].mode == ADDR_REGISTER_IND ||
        ops[1].mode == ADDR_INDEXED) {
        int bit = (int)ops[0].value & 7;
        emit_byte(as, 0xB0);
        emit_mem_operand(as, &ops[1]);
        emit_byte(as, 0xA0 + bit);
        return true;
    }

    error(as, "unsupported SET operand");
    return false;
}

/* RES - Reset bit */
static bool encode_res(Assembler *as, Operand *ops, int count) {
    if (count < 2) {
        error(as, "RES requires bit number and operand");
        return false;
    }

    if (ops[1].mode == ADDR_REGISTER) {
        if (ops[1].size == SIZE_BYTE) {
            int bit = (int)ops[0].value & 7;
            int code = get_reg8_code(ops[1].reg);
            if (code >= 0) {
                emit_byte(as, 0xC8 + (code >> 1));
                emit_byte(as, 0x78 + (code & 1));
                emit_byte(as, bit);
                return true;
            }
        }
        if (ops[1].size == SIZE_WORD) {
            int bit = (int)ops[0].value & 15;
            int code = get_reg16_code(ops[1].reg);
            if (code >= 0) {
                emit_byte(as, 0xE8 + code);
                emit_byte(as, 0x38);
                emit_byte(as, bit);
                return true;
            }
        }
    }

    /* RES n, (mem) */
    if (ops[1].mode == ADDR_DIRECT || ops[1].mode == ADDR_REGISTER_IND ||
        ops[1].mode == ADDR_INDEXED) {
        int bit = (int)ops[0].value & 7;
        emit_byte(as, 0xB0);
        emit_mem_operand(as, &ops[1]);
        emit_byte(as, 0xB0 + bit);
        return true;
    }

    error(as, "unsupported RES operand");
    return false;
}

/* TSET - Test and set bit */
static bool encode_tset(Assembler *as, Operand *ops, int count) {
    if (count < 2) {
        error(as, "TSET requires bit number and operand");
        return false;
    }

    int bit = (int)ops[0].value & 7;

    if (ops[1].mode == ADDR_REGISTER) {
        if (ops[1].size == SIZE_BYTE) {
            int code = get_reg8_code(ops[1].reg);
            if (code >= 0) {
                emit_byte(as, 0xC8 + (code >> 1));
                emit_byte(as, 0xA0 + (code & 1));
                emit_byte(as, bit);
                return true;
            }
        }
    }

    error(as, "unsupported TSET operand");
    return false;
}

/* CHG - Change (toggle) bit */
static bool encode_chg(Assembler *as, Operand *ops, int count) {
    if (count < 2) {
        error(as, "CHG requires bit number and operand");
        return false;
    }

    int bit = (int)ops[0].value & 7;

    if (ops[1].mode == ADDR_REGISTER) {
        if (ops[1].size == SIZE_BYTE) {
            int code = get_reg8_code(ops[1].reg);
            if (code >= 0) {
                emit_byte(as, 0xC8 + (code >> 1));
                emit_byte(as, 0xA8 + (code & 1));
                emit_byte(as, bit);
                return true;
            }
        }
    }

    error(as, "unsupported CHG operand");
    return false;
}

/* STCF - Store Carry Flag to bit */
static bool encode_stcf(Assembler *as, Operand *ops, int count) {
    if (count < 2) {
        error(as, "STCF requires bit and operand");
        return false;
    }

    /* STCF A, (mem) - store CF to bit specified by A */
    if (ops[0].mode == ADDR_REGISTER && ops[0].reg == REG_A) {
        if (ops[1].mode == ADDR_DIRECT || ops[1].mode == ADDR_REGISTER_IND ||
            ops[1].mode == ADDR_INDEXED) {
            emit_byte(as, 0xB0);  /* Bit operation memory prefix */
            emit_mem_operand(as, &ops[1]);
            emit_byte(as, 0x34);
            return true;
        }
    }

    /* STCF n, reg - store CF to bit n of register */
    if (ops[0].mode == ADDR_IMMEDIATE && ops[1].mode == ADDR_REGISTER) {
        if (ops[1].size == SIZE_BYTE) {
            int bit = (int)ops[0].value & 7;
            int code = get_reg8_code(ops[1].reg);
            if (code >= 0) {
                emit_byte(as, 0xC8 + (code >> 1));
                emit_byte(as, 0x30 + (code & 1));
                emit_byte(as, bit);
                return true;
            }
        }
        if (ops[1].size == SIZE_WORD) {
            int bit = (int)ops[0].value & 15;
            int code = get_reg16_code(ops[1].reg);
            if (code >= 0) {
                emit_byte(as, 0xE8 + code);
                emit_byte(as, 0x10);
                emit_byte(as, bit);
                return true;
            }
        }
    }

    error(as, "unsupported STCF operand");
    return false;
}

/* ============== Extension Instructions ============== */

/* EXTZ - Extend with zeros */
static bool encode_extz(Assembler *as, Operand *ops, int count) {
    if (count < 1) {
        error(as, "EXTZ requires a register");
        return false;
    }

    if (ops[0].mode == ADDR_REGISTER) {
        if (ops[0].size == SIZE_WORD) {
            int code = get_reg16_code(ops[0].reg);
            if (code >= 0) {
                emit_byte(as, 0xD8 + code);
                emit_byte(as, 0x12);
                return true;
            }
        } else if (ops[0].size == SIZE_LONG) {
            int code = get_reg32_code(ops[0].reg);
            if (code >= 0) {
                emit_byte(as, 0xE8 + code);
                emit_byte(as, 0x12);
                return true;
            }
        }
    }

    error(as, "unsupported EXTZ operand");
    return false;
}

/* EXTS - Extend with sign */
static bool encode_exts(Assembler *as, Operand *ops, int count) {
    if (count < 1) {
        error(as, "EXTS requires a register");
        return false;
    }

    if (ops[0].mode == ADDR_REGISTER) {
        if (ops[0].size == SIZE_WORD) {
            int code = get_reg16_code(ops[0].reg);
            if (code >= 0) {
                emit_byte(as, 0xD8 + code);
                emit_byte(as, 0x13);
                return true;
            }
        } else if (ops[0].size == SIZE_LONG) {
            int code = get_reg32_code(ops[0].reg);
            if (code >= 0) {
                emit_byte(as, 0xE8 + code);
                emit_byte(as, 0x13);
                return true;
            }
        }
    }

    error(as, "unsupported EXTS operand");
    return false;
}

/* SCC - Set on condition code */
static bool encode_scc(Assembler *as, Operand *ops, int count) {
    if (count < 2) {
        error(as, "SCC requires condition and register");
        return false;
    }

    int cc;

    if (ops[0].mode == ADDR_CONDITION) {
        cc = get_cc_code(ops[0].value);
    } else if (ops[0].mode == ADDR_REGISTER && ops[0].size == SIZE_BYTE) {
        /* C, Z registers are also condition codes - for SCC, treat them as conditions */
        if (ops[0].reg == REG_C) {
            cc = get_cc_code(CC_C);  /* Carry */
        } else {
            error(as, "SCC first operand must be a condition");
            return false;
        }
    } else {
        error(as, "SCC first operand must be a condition");
        return false;
    }

    if (ops[1].mode == ADDR_REGISTER) {
        if (ops[1].size == SIZE_BYTE) {
            int code = get_reg8_code(ops[1].reg);
            if (code >= 0) {
                emit_byte(as, 0xC8 + (code >> 1));
                emit_byte(as, 0x70 + cc);
                return true;
            }
        }
        if (ops[1].size == SIZE_WORD) {
            int code = get_reg16_code(ops[1].reg);
            if (code >= 0) {
                emit_byte(as, 0xE8 + code);
                emit_byte(as, 0x70 + cc);
                return true;
            }
        }
    }

    error(as, "unsupported SCC operand");
    return false;
}

/* ============== Instruction Table ============== */

typedef bool (*EncoderFunc)(Assembler *, Operand *, int);

static const struct {
    const char *mnemonic;
    EncoderFunc encoder;
} instruction_table[] = {
    /* System */
    {"NOP", encode_nop},
    {"EI", encode_ei},
    {"DI", encode_di},
    {"HALT", encode_halt},
    {"SCF", encode_scf},
    {"RCF", encode_rcf},
    {"CCF", encode_ccf},
    {"ZCF", encode_zcf},

    /* Stack */
    {"PUSH", encode_push},
    {"PUSHW", encode_pushw},
    {"POP", encode_pop},
    {"LINK", encode_link},
    {"UNLK", encode_unlk},

    /* Control flow */
    {"RET", encode_ret},
    {"RETI", encode_reti},
    {"RETD", encode_retd},
    {"SWI", encode_swi},
    {"JP", encode_jp},
    {"JR", encode_jr},
    {"JRL", encode_jrl},
    {"CALL", encode_call},
    {"CALR", encode_calr},
    {"DJNZ", encode_djnz},

    /* Data movement */
    {"LD", encode_ld},
    {"LDA", encode_lda},
    {"LDC", encode_ldc},
    {"LDI", encode_ldi},
    {"LDIR", encode_ldir},
    {"LDDR", encode_lddr},
    {"LDIW", encode_ldiw},
    {"LDIRW", encode_ldirw},
    {"LDDRW", encode_lddrw},
    {"LDW", encode_ldw},
    {"EX", encode_ex},

    /* Arithmetic */
    {"ADD", encode_add},
    {"ADC", encode_adc},
    {"SUB", encode_sub},
    {"SBC", encode_sbc},
    {"CP", encode_cp},
    {"CPW", encode_cpw},
    {"INC", encode_inc},
    {"INCW", encode_incw},
    {"DEC", encode_dec},
    {"DECW", encode_decw},
    {"NEG", encode_neg},
    {"MUL", encode_mul},
    {"MULS", encode_muls},
    {"DIV", encode_div},
    {"DIVS", encode_divs},
    {"DAA", encode_daa},

    /* Logical */
    {"AND", encode_and},
    {"ANDW", encode_andw},
    {"OR", encode_or},
    {"ORW", encode_orw},
    {"XOR", encode_xor},
    {"CPL", encode_cpl},
    {"ADDW", encode_addw},

    /* Shift/Rotate */
    {"RLC", encode_rlc},
    {"RRC", encode_rrc},
    {"RL", encode_rl},
    {"RR", encode_rr},
    {"SLA", encode_sla},
    {"SRA", encode_sra},
    {"SLL", encode_sll},
    {"SRL", encode_srl},

    /* Bit */
    {"BIT", encode_bit},
    {"SET", encode_set},
    {"RES", encode_res},
    {"TSET", encode_tset},
    {"CHG", encode_chg},
    {"STCF", encode_stcf},

    /* Extension */
    {"EXTZ", encode_extz},
    {"EXTS", encode_exts},
    {"SCC", encode_scc},

    {NULL, NULL}
};

/* Main instruction encoder entry point */
bool encode_instruction(Assembler *as, const char *mnemonic, Operand *operands, int operand_count) {
    /* Look up instruction */
    for (int i = 0; instruction_table[i].mnemonic; i++) {
        if (strcasecmp(mnemonic, instruction_table[i].mnemonic) == 0) {
            return instruction_table[i].encoder(as, operands, operand_count);
        }
    }

    /* Not found - might be a macro, let caller handle it */
    return false;
}
