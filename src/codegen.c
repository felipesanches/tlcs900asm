/*
 * TLCS-900 Assembler - Code Generator
 *
 * Instruction encoding for the TLCS-900/H CPU.
 *
 * Encoding patterns:
 * - 8-bit register prefix: 0xC8 + pair (C8=W/A, C9=B/C, CA=D/E, CB=H/L)
 * - 16-bit register prefix: 0xD8 + reg (D8=WA, D9=BC, DA=DE, DB=HL, DC=IX, DD=IY, DE=IZ)
 * - 32-bit register prefix: 0xE8 + reg (E8=XWA, E9=XBC, EA=XDE, EB=XHL, EC=XIX, ED=XIY, EE=XIZ)
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
        case REG_W: return 0; case REG_A: return 1;
        case REG_B: return 2; case REG_C: return 3;
        case REG_D: return 4; case REG_E: return 5;
        case REG_H: return 6; case REG_L: return 7;
        default: return -1;
    }
}

/* Get 16-bit register encoding */
static int get_reg16_code(RegisterType reg) {
    switch (reg) {
        case REG_WA: return 0; case REG_BC: return 1;
        case REG_DE: return 2; case REG_HL: return 3;
        case REG_IX: return 4; case REG_IY: return 5;
        case REG_IZ: return 6; case REG_SP: return 7;
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
    if (code >= 0) {
        return 0xC8 + (code >> 1);  /* Pairs: W/A, B/C, D/E, H/L */
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

/* ============== Instruction Encoders ============== */

/* NOP */
static bool encode_nop(Assembler *as, Operand *ops, int count) {
    (void)ops; (void)count;
    emit_byte(as, 0x00);
    return true;
}

/* EI imm3 - Enable interrupts */
static bool encode_ei(Assembler *as, Operand *ops, int count) {
    int level = 7;  /* Default to enable all */
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
            /* 8-bit push uses prefix */
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

    error(as, "invalid PUSH operand");
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

/* RET */
static bool encode_ret(Assembler *as, Operand *ops, int count) {
    (void)ops; (void)count;
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

    int cc = CC_T;  /* Default to unconditional */
    Operand *target = &ops[0];

    /* Check for condition code as first operand */
    if (count >= 2 && ops[0].mode == ADDR_CONDITION) {
        cc = (int)ops[0].value;
        target = &ops[1];
    }

    /* JP cc, nn - conditional absolute jump */
    if (target->mode == ADDR_IMMEDIATE || target->mode == ADDR_DIRECT) {
        int addr = (int)target->value;
        emit_byte(as, 0xB0 + get_cc_code(cc));
        emit_byte(as, addr & 0xFF);
        emit_byte(as, (addr >> 8) & 0xFF);
        emit_byte(as, (addr >> 16) & 0xFF);
        return true;
    }

    /* JP (mem) - indirect jump */
    if (target->mode == ADDR_REGISTER_IND || target->mode == ADDR_INDEXED ||
        target->mode == ADDR_DIRECT) {
        if (cc != CC_T) {
            error(as, "conditional JP not supported with indirect addressing");
            return false;
        }
        emit_byte(as, 0xB4);
        emit_mem_operand(as, target);
        emit_byte(as, 0xD8);
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

    if (offset >= -128 && offset <= 127) {
        /* Short JR */
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
        emit_byte(as, 0x9A);
        emit_byte(as, get_cc_code(cc));
        emit_byte(as, target->value & 0xFF);
        emit_byte(as, (target->value >> 8) & 0xFF);
        emit_byte(as, (target->value >> 16) & 0xFF);
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
    }

    error(as, "invalid DJNZ register");
    return false;
}

/* LD - Load (this is complex, many forms) */
static bool encode_ld(Assembler *as, Operand *ops, int count) {
    if (count < 2) {
        error(as, "LD requires two operands");
        return false;
    }

    Operand *dst = &ops[0];
    Operand *src = &ops[1];

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
         dst->mode == ADDR_DIRECT || dst->mode == ADDR_REGISTER_IND_DEC) &&
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
         dst->mode == ADDR_DIRECT) && src->mode == ADDR_IMMEDIATE) {
        /* Need to determine size from context or directive */
        emit_byte(as, 0x80);
        emit_mem_operand(as, dst);
        emit_byte(as, 0x00);
        emit_byte(as, (uint8_t)src->value);
        return true;
    }

    error(as, "unsupported LD operand combination");
    return false;
}

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
        }
    }

    error(as, "unsupported ADD operand combination");
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

    error(as, "unsupported SUB operand combination");
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

    error(as, "unsupported CP operand combination");
    return false;
}

/* INC */
static bool encode_inc(Assembler *as, Operand *ops, int count) {
    if (count < 1) {
        error(as, "INC requires an operand");
        return false;
    }

    int inc_val = 1;
    if (count >= 2 && ops[1].mode == ADDR_IMMEDIATE) {
        inc_val = (int)ops[1].value;
    }

    if (ops[0].mode == ADDR_REGISTER) {
        if (ops[0].size == SIZE_BYTE) {
            int code = get_reg8_code(ops[0].reg);
            if (code >= 0) {
                emit_byte(as, 0xC8 + (code >> 1));
                emit_byte(as, 0x60 + (code & 1));
                emit_byte(as, (uint8_t)inc_val);
                return true;
            }
        } else if (ops[0].size == SIZE_WORD) {
            int code = get_reg16_code(ops[0].reg);
            if (code >= 0) {
                emit_byte(as, 0xD8 + code);
                emit_byte(as, 0x60);
                emit_byte(as, (uint8_t)inc_val);
                return true;
            }
        } else if (ops[0].size == SIZE_LONG) {
            int code = get_reg32_code(ops[0].reg);
            if (code >= 0) {
                emit_byte(as, 0xE8 + code);
                emit_byte(as, 0x60);
                emit_byte(as, (uint8_t)inc_val);
                return true;
            }
        }
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
    if (count >= 2 && ops[1].mode == ADDR_IMMEDIATE) {
        dec_val = (int)ops[1].value;
    }

    if (ops[0].mode == ADDR_REGISTER) {
        if (ops[0].size == SIZE_BYTE) {
            int code = get_reg8_code(ops[0].reg);
            if (code >= 0) {
                emit_byte(as, 0xC8 + (code >> 1));
                emit_byte(as, 0x68 + (code & 1));
                emit_byte(as, (uint8_t)dec_val);
                return true;
            }
        } else if (ops[0].size == SIZE_WORD) {
            int code = get_reg16_code(ops[0].reg);
            if (code >= 0) {
                emit_byte(as, 0xD8 + code);
                emit_byte(as, 0x68);
                emit_byte(as, (uint8_t)dec_val);
                return true;
            }
        } else if (ops[0].size == SIZE_LONG) {
            int code = get_reg32_code(ops[0].reg);
            if (code >= 0) {
                emit_byte(as, 0xE8 + code);
                emit_byte(as, 0x68);
                emit_byte(as, (uint8_t)dec_val);
                return true;
            }
        }
    }

    error(as, "unsupported DEC operand");
    return false;
}

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
        }
    }

    error(as, "unsupported OR operand combination");
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
        }
    }

    error(as, "unsupported XOR operand combination");
    return false;
}

/* Instruction encoding table */
typedef bool (*EncoderFunc)(Assembler *, Operand *, int);

static const struct {
    const char *mnemonic;
    EncoderFunc encoder;
} instruction_table[] = {
    {"NOP", encode_nop},
    {"EI", encode_ei},
    {"DI", encode_di},
    {"HALT", encode_halt},
    {"SCF", encode_scf},
    {"RCF", encode_rcf},
    {"CCF", encode_ccf},
    {"ZCF", encode_zcf},
    {"PUSH", encode_push},
    {"POP", encode_pop},
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
    {"LD", encode_ld},
    {"ADD", encode_add},
    {"SUB", encode_sub},
    {"CP", encode_cp},
    {"INC", encode_inc},
    {"DEC", encode_dec},
    {"AND", encode_and},
    {"OR", encode_or},
    {"XOR", encode_xor},
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

    /* Check if it might be a macro call - for now, error */
    error(as, "unknown instruction '%s'", mnemonic);
    return false;
}
