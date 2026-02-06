# Makefile for libtms9900 runtime library (builtins)
#
# Uses LLVM/Clang TMS9900 backend
#
# Usage:
#   make          - Build all object files
#   make clean    - Remove generated files

CC = ../../llvm-project/build/bin/clang
AR = ../../llvm-project/build/bin/llvm-ar

ASFLAGS = --target=tms9900 -c

# Source files
SRCS = mul32.S div32.S shift32.S memcpy.S memset.S memmove.S

# Object files
OBJS = $(SRCS:.S=.o)

BUILD = build

# Default target
all: $(BUILD)/libbuiltins.a

$(BUILD):
	mkdir -p $(BUILD)

# Pattern rule for assembling
$(BUILD)/%.o: %.S | $(BUILD)
	$(CC) $(ASFLAGS) -o $@ $<

# Clean up
clean:
	rm -rf $(BUILD)

$(BUILD)/libbuiltins.a: $(OBJS:%.o=$(BUILD)/%.o)
	$(AR) rcs $@ $^

# Show what would be built
info:
	@echo "Sources: $(SRCS)"
	@echo "Objects: $(OBJS)"
	@echo "Assembler: $(CC)"

.PHONY: all clean info
