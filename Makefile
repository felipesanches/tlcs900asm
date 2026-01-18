# TLCS-900/TMP94C241 Assembler Makefile

CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -O2 -g
LDFLAGS =

# Directories
SRCDIR = src
INCDIR = include
OBJDIR = obj

# Source files
SRCS = $(wildcard $(SRCDIR)/*.c)
OBJS = $(patsubst $(SRCDIR)/%.c,$(OBJDIR)/%.o,$(SRCS))

# Target
TARGET = tlcs900asm

.PHONY: all clean test

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $^

$(OBJDIR)/%.o: $(SRCDIR)/%.c | $(OBJDIR)
	$(CC) $(CFLAGS) -I$(INCDIR) -c -o $@ $<

$(OBJDIR):
	mkdir -p $(OBJDIR)

clean:
	rm -rf $(OBJDIR) $(TARGET)

# Test with a simple file
test: $(TARGET)
	@echo "Testing with simple NOP..."
	@echo "NOP" > /tmp/test.asm
	./$(TARGET) -v /tmp/test.asm -o /tmp/test.rom
	@hexdump -C /tmp/test.rom
	@rm -f /tmp/test.asm /tmp/test.rom

# Debug build
debug: CFLAGS += -DDEBUG -O0
debug: clean all

# Install to /usr/local/bin
install: $(TARGET)
	install -m 755 $(TARGET) /usr/local/bin/
