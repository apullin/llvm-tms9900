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
I64_CFLAGS = $(CFLAGS)
MULCHK_CFLAGS = $(CFLAGS)
RT_CFLAGS = $(CFLAGS)

# Assembly source files (32-bit builtins + memops + bitops)
ASM_SRCS = mul32.S div32.S shift32.S memcpy.S memset.S memmove.S \
           bswapsi2.S clzsi2.S ctzsi2.S

# Assembly source files: 64-bit builtins + extended 32-bit ops
# (replacing all compiler-rt C implementations)
ASM_EXT_SRCS = ashldi3.S negdi2.S cmpdi2.S clzdi2.S ctzdi2.S \
               ffssi2.S paritysi2.S popcountsi2.S bswapdi2.S \
               absvsi2.S absvdi2.S addvsi3.S addvdi3.S \
               mulvsi3.S divmodsi4.S

# C source files (64-bit builtins)
I64_SRCS = i64/muldi3.c i64/udivmoddi4.c i64/divdi3.c i64/udivdi3.c \
           i64/moddi3.c i64/umoddi3.c

# C source files: checked multiply (too complex for pure ASM)
MULCHK_SRCS = mulvdi3.c mulosi4.c

# C source files: IEEE-754 single-precision compiler builtins
SOFTFLOAT_SRCS = softfloat/addsf3.c softfloat/subsf3.c \
                 softfloat/mulsf3.c softfloat/divsf3.c \
                 softfloat/negsf2.c softfloat/comparesf2.c \
                 softfloat/floatsisf.c softfloat/floatunsisf.c \
                 softfloat/fixsfsi.c softfloat/fixunssfsi.c \
                 softfloat/extendhfsf2.c softfloat/truncsfhf2.c \
                 softfloat/fenv.c

# C source files (runtime support)
RT_SRCS = malloc.c string_utils.c

# Object files
ASM_OBJS = $(ASM_SRCS:.S=.o)
ASM_EXT_OBJS = $(ASM_EXT_SRCS:.S=.o)
I64_OBJS = $(notdir $(I64_SRCS:.c=.o))
MULCHK_OBJS = $(MULCHK_SRCS:.c=.o)
SOFTFLOAT_OBJS = $(notdir $(SOFTFLOAT_SRCS:.c=.o))
RT_OBJS = $(RT_SRCS:.c=.o)
ALL_OBJS = $(ASM_OBJS) $(ASM_EXT_OBJS) $(I64_OBJS) $(MULCHK_OBJS) \
           $(SOFTFLOAT_OBJS) $(RT_OBJS)

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
	$(CC) $(I64_CFLAGS) -o $@ $<

# Pattern rule for compiling single-precision soft-float sources
$(BUILD)/%.o: softfloat/%.c | $(BUILD)
	$(CC) $(CFLAGS) -o $@ $<

# Pattern rule for compiling runtime C sources
$(BUILD)/mulvdi3.o: mulvdi3.c | $(BUILD)
	$(CC) $(MULCHK_CFLAGS) -o $@ $<

$(BUILD)/mulosi4.o: mulosi4.c | $(BUILD)
	$(CC) $(MULCHK_CFLAGS) -o $@ $<

$(BUILD)/malloc.o: malloc.c | $(BUILD)
	$(CC) $(RT_CFLAGS) -o $@ $<

$(BUILD)/string_utils.o: string_utils.c | $(BUILD)
	$(CC) $(RT_CFLAGS) -o $@ $<

$(BUILD)/%.o: %.c | $(BUILD)
	$(CC) $(CFLAGS) -o $@ $<

# Clean up
clean:
	rm -rf $(BUILD)

$(BUILD)/libbuiltins.a: $(ALL_OBJS:%.o=$(BUILD)/%.o)
	$(AR) rcs $@ $^

# Show what would be built
info:
	@echo "ASM Sources: $(ASM_SRCS)"
	@echo "ASM Ext Sources: $(ASM_EXT_SRCS)"
	@echo "I64 Sources: $(I64_SRCS)"
	@echo "MulChk Sources: $(MULCHK_SRCS)"
	@echo "Soft-float Sources: $(SOFTFLOAT_SRCS)"
	@echo "All Objects: $(ALL_OBJS)"
	@echo "Compiler: $(CC)"

RESOURCE_DIR := $(shell $(CC) --print-resource-dir)

install: $(BUILD)/libbuiltins.a
	mkdir -p $(RESOURCE_DIR)/lib
	cp $(BUILD)/libbuiltins.a $(RESOURCE_DIR)/lib/libclang_rt.builtins.a
	cp $(BUILD)/libbuiltins.a $(RESOURCE_DIR)/lib/libclang_rt.builtins-tms9900.a

.PHONY: all clean info install
