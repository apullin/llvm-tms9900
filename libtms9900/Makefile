# Makefile for libtms9900 runtime library
#
# Requires xas99 from xdt99 tools (https://github.com/endlos99/xdt99)
#
# Usage:
#   make          - Build all object files
#   make clean    - Remove generated files

XAS99 ?= xas99.py
XASFLAGS = -R

# Source files
SRCS = mul32.asm div32.asm shift32.asm

# Object files
OBJS = $(SRCS:.asm=.o)

# Default target
all: $(OBJS)

# Pattern rule for assembling
%.o: %.asm
	$(XAS99) $(XASFLAGS) -o $@ $<

# Individual dependencies (if needed)
mul32.o: mul32.asm
div32.o: div32.asm
shift32.o: shift32.asm

# Clean up
clean:
	rm -f $(OBJS) *.lst

# Show what would be built
info:
	@echo "Sources: $(SRCS)"
	@echo "Objects: $(OBJS)"
	@echo "Assembler: $(XAS99)"

.PHONY: all clean info
