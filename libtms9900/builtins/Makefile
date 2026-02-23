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
CFLAGS = --target=tms9900 -O2 -fno-builtin -c

# compiler-rt source directory
CRT_DIR = ../../llvm-project/compiler-rt/lib/builtins
CRT_CFLAGS = --target=tms9900 -O2 -ffreestanding -fno-builtin -I$(CRT_DIR) -c

# Assembly source files (32-bit builtins + memops)
ASM_SRCS = mul32.S div32.S shift32.S memcpy.S memset.S memmove.S

# C source files (64-bit builtins)
I64_SRCS = i64/muldi3.c i64/udivmoddi4.c i64/divdi3.c i64/udivdi3.c \
           i64/moddi3.c i64/umoddi3.c

# C source files (runtime support)
RT_SRCS = malloc.c string_utils.c

# compiler-rt C source files (cross-compiled from upstream)
CRT_SRCS = absvsi2.c absvdi2.c \
           addvsi3.c addvdi3.c \
           subvsi3.c subvdi3.c \
           negvsi2.c negvdi2.c \
           mulvsi3.c mulvdi3.c \
           mulosi4.c mulodi4.c \
           bswapsi2.c bswapdi2.c \
           ctzsi2.c ctzdi2.c \
           clzsi2.c clzdi2.c \
           ffssi2.c ffsdi2.c \
           paritysi2.c paritydi2.c \
           popcountsi2.c popcountdi2.c \
           cmpdi2.c ucmpdi2.c \
           negdi2.c \
           udivmodsi4.c divmodsi4.c \
           ashldi3.c ashrdi3.c lshrdi3.c

# Object files
ASM_OBJS = $(ASM_SRCS:.S=.o)
I64_OBJS = $(notdir $(I64_SRCS:.c=.o))
RT_OBJS = $(RT_SRCS:.c=.o)
CRT_OBJS = $(addprefix crt_,$(CRT_SRCS:.c=.o))
ALL_OBJS = $(ASM_OBJS) $(I64_OBJS) $(RT_OBJS) $(CRT_OBJS)

BUILD = build

# Default target
all: $(BUILD)/libbuiltins.a

$(BUILD):
	mkdir -p $(BUILD)

# Pattern rule for assembling
$(BUILD)/%.o: %.S | $(BUILD)
	$(CC) $(ASFLAGS) -o $@ $<

# Pattern rule for compiling i64 C sources
$(BUILD)/%.o: i64/%.c | $(BUILD)
	$(CC) $(CFLAGS) -o $@ $<

# Pattern rule for compiling runtime C sources
$(BUILD)/%.o: %.c | $(BUILD)
	$(CC) $(CFLAGS) -o $@ $<

# Pattern rule for compiling compiler-rt sources (prefixed with crt_ to avoid conflicts)
$(BUILD)/crt_%.o: $(CRT_DIR)/%.c | $(BUILD)
	$(CC) $(CRT_CFLAGS) -o $@ $<

# Clean up
clean:
	rm -rf $(BUILD)

$(BUILD)/libbuiltins.a: $(ALL_OBJS:%.o=$(BUILD)/%.o)
	$(AR) rcs $@ $^

# Show what would be built
info:
	@echo "ASM Sources: $(ASM_SRCS)"
	@echo "I64 Sources: $(I64_SRCS)"
	@echo "CRT Sources: $(CRT_SRCS)"
	@echo "All Objects: $(ALL_OBJS)"
	@echo "Compiler: $(CC)"

.PHONY: all clean info
