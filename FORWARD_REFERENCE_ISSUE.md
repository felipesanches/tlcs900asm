# Forward Reference and JR Offset Issue

## Problem Description

The tlcs900asm assembler experiences JR (jump relative) offset out-of-range errors when assembling the KN5000 subcpu ROM. This is caused by inconsistent instruction sizes between pass 1 and pass 2 of assembly.

## Root Cause

The assembler uses a two-pass approach:

### Pass 1: Symbol Collection
- Labels are defined as they're encountered
- Forward references (symbols not yet defined) get value 0
- Instruction sizes are calculated based on available information

### Pass 2: Code Generation
- All symbols are now known with actual values
- Instruction sizes are recalculated with real values
- Code is emitted

### The Size Mismatch Problem

For instructions with variable-sized operands (like direct memory addressing):
- **8-bit addressing**: 2 bytes (C0 + addr)
- **16-bit addressing**: 3 bytes (D0 + addr_lo + addr_hi)
- **24-bit addressing**: 4 bytes (E0 + addr_lo + addr_mid + addr_hi)

When a forward reference is encountered in pass 1:
1. The symbol value is unknown (defaults to 0)
2. Value 0 fits in 8 bits, so 8-bit encoding is chosen (2 bytes)

In pass 2:
1. The symbol now has its actual value (e.g., 0x20EFF)
2. Value > 255, so 16-bit encoding is needed (3 bytes)

This 1-byte difference per affected instruction accumulates, shifting all subsequent label addresses. JR instructions that were within range (±127 bytes) in pass 1 may be out of range in pass 2.

## Attempted Solutions

### 1. Conservative Sizing for Forward References
```c
if (!op->value_known) {
    addr_size = 16;  // Always use 16-bit for unknown values
}
```
**Result**: Made the ROM larger, causing different JR offsets to go out of range.

### 2. is_constant Tracking
Added tracking to distinguish between:
- Constants (EQU symbols, literals) - use optimal sizing
- Labels - always use 16-bit minimum

**Result**: Still caused ROM growth because backward references to labels also used 16-bit.

### 3. Optimal Sizing Only
Use optimal sizing based on actual value, regardless of whether it's a forward reference.

**Result**: Size mismatches between passes cause the JR errors.

## Proper Solutions

### Option 1: Three-Pass Assembly
1. **Pass 1**: Collect all symbol addresses using conservative size estimates
2. **Pass 2**: With all symbols known, recalculate instruction sizes
3. **Pass 3**: Emit code with final, stable sizes

### Option 2: Iterative Size Calculation
1. Initial pass with conservative sizes
2. Recalculate sizes until they stabilize (no changes between iterations)
3. Emit code

### Option 3: Fixup Records
1. Emit code with maximum-size placeholders
2. Record locations that need fixups
3. After all code is emitted, shrink instructions where possible and adjust offsets

### Option 4: Relaxation Algorithm
Similar to how linkers handle this:
1. Start with minimum sizes
2. If any branch is out of range, enlarge it and recalculate
3. Repeat until all branches fit

## ASL Compatibility Note

The original ROM was assembled with ASL (Alfred Arnold's Macro Assembler), which correctly handles forward references. ASL likely uses one of the above approaches internally.

## Files Affected

- `/home/fsanches/devel/Projeto_KN5000/tlcs900asm/src/codegen.c` - emit_mem_operand() function
- `/home/fsanches/devel/Projeto_KN5000/tlcs900asm/src/assembler.c` - pass handling
- `/home/fsanches/devel/Projeto_KN5000/tlcs900asm/src/expressions.c` - value_known tracking

## Current Workaround

None available. The assembler cannot currently produce a byte-accurate ROM due to this issue.

## Technical Details

### Affected Instructions (examples from subcpu ROM)
- Line 11137: `JR NZ, LABEL_020EFF` (INT0_HANDLER)
- 51 total JR instructions fail with "offset out of range"

### Memory Operand Encoding (TLCS-900)
```
C0 xx       - 8-bit direct address (2 bytes)
D0 xx xx    - 16-bit direct address (3 bytes)
E0 xx xx xx - 24-bit direct address (4 bytes)
```

### JR Instruction Range
- Signed 8-bit offset: -128 to +127 bytes from end of instruction
- If target moves beyond this range, JRL (long jump) must be used instead

## Date
2026-01-19
