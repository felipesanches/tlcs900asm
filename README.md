# TLCS-900 Assembler

This project is released into the public domain. See the [LICENSE](LICENSE) file for details.

A two-pass assembler for the Toshiba TLCS-900/TMP94C241 CPU, designed as a drop-in replacement for ASL (Alfred's Macro Assembler) for assembling KN5000 subcpu firmware.

## Building

```bash
make
```

## Usage

```bash
./tlcs900asm input.asm -o output.rom [-v]
```

Options:
- `-o <file>`: Output file (required)
- `-v`: Verbose mode

## Features

### Supported Instructions

- **Data Transfer**: LD, LDA, LDAR, LDC, LDI, LDIR, LDD, LDDR, PUSH, POP, EX, MIRR
- **Arithmetic**: ADD, ADC, SUB, SBC, INC, DEC, NEG, MUL, MULS, DIV, DIVS, DAA
- **Logical**: AND, OR, XOR, CPL, ANDW, ORW, XORW
- **Bit Operations**: BIT, SET, RES, TSET, CHG, STCF, LDCF, XORCF, BS1B, BS1F
- **Shift/Rotate**: RLC, RRC, RL, RR, SLA, SRA, SRL, RLD, RRD
- **Control Flow**: JP, JR, JRL, CALL, CALR, RET, RETI, DJNZ
- **Comparison**: CP
- **Extension**: EXTZ, EXTS, SCC
- **System**: NOP, EI, DI, HALT, SWI

### Addressing Modes

- Register direct (8/16/32-bit)
- Register indirect: `(XWA)`, `(XIX)`
- Indexed: `(XIX + offset)`, `(XIX + BC)`
- Pre-decrement: `(-XIX)`
- Post-increment: `(XIX+)`
- Direct memory: `(address)`
- Immediate: `#value` or just `value`
- Address size suffixes: `:8`, `:16`, `:24`

### Directives

- `CPU TLCS900` - Set CPU type
- `ORG address` - Set origin
- `EQU` / `=` - Define constant
- `DB` / `DEFB` / `DCB` - Define bytes
- `DW` / `DEFW` / `DCW` - Define words
- `DD` / `DEFD` / `DCD` - Define double words
- `DS` / `DEFS` / `RMB` - Reserve space
- `INCLUDE "file"` - Include source file
- `MACRO` / `ENDM` - Macro definition
- `IF` / `ELSE` / `ENDIF` - Conditional assembly
- `ALIGN n` - Align to boundary
- `PHASE` / `DEPHASE` - Address remapping
- `END` - End of source

### Special Features

- **Control Register Support**: LDC instruction supports DMA control registers (DMAS0-3, DMAD0-3, DMAC0-3, DMAM0-3)
- **Symbol Precedence**: User-defined symbols take precedence over register names in indirect addressing
- **Label Tolerance**: Accepts labels without trailing colons (common in disassembly output)
- **Error Recovery**: Continues assembly after errors to report all issues

## Current Status

The assembler successfully processes the KN5000 subcpu disassembly with only **8 errors** remaining, all of which are source file issues (not assembler bugs):

| Line | Issue | Description |
|------|-------|-------------|
| 8983 | `LD (1036h), (SC1CR)` | Memory-to-memory LD not supported by TLCS-900 |
| 40000 | `LDW_16_16 (044b4h), (045b0h)` | Undefined macro |
| 51104 | `LD (4A48h), (XBC + 001h)` | Memory-to-indexed LD not supported |

### To Fix Source File Issues

1. Replace memory-to-memory transfers with register-intermediate sequences:
   ```asm
   ; Instead of: LD (1036h), (SC1CR)
   LD A, (SC1CR)
   LD (1036h), A
   ```

2. Define the `LDW_16_16` macro or replace with equivalent instructions.

## Architecture

The assembler uses a standard two-pass approach:

1. **Pass 1**: Collect labels and symbols, calculate addresses
2. **Pass 2**: Generate machine code with resolved references

Source files:
- `src/main.c` - Entry point and argument parsing
- `src/assembler.c` - Two-pass assembly driver
- `src/lexer.c` - Tokenizer
- `src/parser.c` - Line parser and operand handling
- `src/expressions.c` - Expression evaluator
- `src/codegen.c` - Instruction encoding
- `src/directives.c` - Directive handling
- `src/symbols.c` - Symbol table
- `src/macros.c` - Macro processor
- `src/output.c` - Binary output
- `src/errors.c` - Error reporting

## License

See the [LICENSE](LICENSE) file for details.
