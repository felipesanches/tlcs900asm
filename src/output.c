/*
 * TLCS-900 Assembler - Binary Output Writer
 *
 * Handles output buffer management and writing to binary files.
 * Supports non-contiguous ORG regions by tracking base address.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../include/tlcs900.h"

#define INITIAL_OUTPUT_SIZE 65536

/* Initialize output buffer */
void output_init(Assembler *as) {
    as->output = malloc(INITIAL_OUTPUT_SIZE);
    if (!as->output) {
        fprintf(stderr, "Failed to allocate output buffer\n");
        exit(1);
    }
    as->output_size = 0;
    as->output_capacity = INITIAL_OUTPUT_SIZE;
    as->output_base = 0;
}

/* Free output buffer */
void output_free(Assembler *as) {
    if (as->output) {
        free(as->output);
        as->output = NULL;
    }
}

/* Ensure capacity for n more bytes */
static void ensure_capacity(Assembler *as, size_t n) {
    size_t needed = as->output_size + n;
    if (needed <= as->output_capacity) return;

    size_t new_cap = as->output_capacity;
    while (new_cap < needed) {
        new_cap *= 2;
    }

    uint8_t *new_buf = realloc(as->output, new_cap);
    if (!new_buf) {
        fprintf(stderr, "Failed to grow output buffer to %zu bytes\n", new_cap);
        exit(1);
    }

    as->output = new_buf;
    as->output_capacity = new_cap;
}

/* Set the base address (first ORG) */
void output_set_base(Assembler *as, uint32_t base) {
    if (as->output_size == 0) {
        as->output_base = base;
    }
}

/* Get current output offset for a given PC */
static size_t pc_to_offset(Assembler *as, uint32_t pc) {
    return pc - as->output_base;
}

/* Emit a single byte */
void emit_byte(Assembler *as, uint8_t b) {
    if (as->pass != 2) {
        as->pc++;
        return;
    }

    size_t offset = pc_to_offset(as, as->pc);

    /* Extend buffer with zeros if needed */
    if (offset >= as->output_size) {
        size_t gap = offset - as->output_size + 1;
        ensure_capacity(as, gap);
        memset(as->output + as->output_size, 0, gap);
        as->output_size = offset + 1;
    } else {
        ensure_capacity(as, 1);
        if (offset + 1 > as->output_size) {
            as->output_size = offset + 1;
        }
    }

    as->output[offset] = b;
    as->pc++;
}

/* Emit a 16-bit word (little-endian) */
void emit_word(Assembler *as, uint16_t w) {
    emit_byte(as, w & 0xFF);
    emit_byte(as, (w >> 8) & 0xFF);
}

/* Emit a 24-bit value (little-endian) */
void emit_word24(Assembler *as, uint32_t w) {
    emit_byte(as, w & 0xFF);
    emit_byte(as, (w >> 8) & 0xFF);
    emit_byte(as, (w >> 16) & 0xFF);
}

/* Emit a 32-bit long (little-endian) */
void emit_long(Assembler *as, uint32_t l) {
    emit_byte(as, l & 0xFF);
    emit_byte(as, (l >> 8) & 0xFF);
    emit_byte(as, (l >> 16) & 0xFF);
    emit_byte(as, (l >> 24) & 0xFF);
}

/* Emit n bytes of padding */
void emit_fill(Assembler *as, size_t count, uint8_t value) {
    for (size_t i = 0; i < count; i++) {
        emit_byte(as, value);
    }
}

/* Emit a string without null terminator */
void emit_string(Assembler *as, const char *str, size_t len) {
    for (size_t i = 0; i < len; i++) {
        emit_byte(as, (uint8_t)str[i]);
    }
}

/* Write output buffer to file */
bool assembler_write_output(Assembler *as, const char *filename) {
    if (as->output_size == 0) {
        fprintf(stderr, "Warning: no output generated\n");
    }

    FILE *fp = fopen(filename, "wb");
    if (!fp) {
        fprintf(stderr, "Error: cannot open output file '%s'\n", filename);
        return false;
    }

    size_t written = fwrite(as->output, 1, as->output_size, fp);
    fclose(fp);

    if (written != as->output_size) {
        fprintf(stderr, "Error: failed to write all bytes to '%s'\n", filename);
        return false;
    }

    if (as->verbose) {
        printf("Wrote %zu bytes to %s (base address $%06X)\n",
               as->output_size, filename, as->output_base);
    }

    return true;
}
