# Makefile for TMS9900 benchmark suite
#
# Usage:
#   make              - Build all benchmarks at -O2
#   make OPT=0        - Build at -O0
#   make fib          - Build single benchmark
#   make clean        - Remove build artifacts

ROOT     = ../..
LLVM_BIN = $(ROOT)/llvm-project/build/bin
CC       = $(LLVM_BIN)/clang
LD       = $(LLVM_BIN)/ld.lld
OBJCOPY  = $(LLVM_BIN)/llvm-objcopy
SIZE     = $(LLVM_BIN)/llvm-size

TARGET   = --target=tms9900
OPT      ?= 2
# -mno-relax-all needed at -O0 to prevent "fixup value out of range" on large functions
ifeq ($(OPT),0)
EXTRA_CFLAGS = -mno-relax-all
endif
CFLAGS   = $(TARGET) -O$(OPT) -ffreestanding -fno-builtin -nostdlib $(EXTRA_CFLAGS)
LDFLAGS  = -T bench.ld

# Softfloat/runtime always compiled at -O2 (matches real-world precompiled library)
SF_CFLAGS = $(TARGET) -O2 -ffreestanding -fno-builtin -nostdlib

BUILD    = build/O$(OPT)

# Builtins library
BUILTINS = $(ROOT)/libtms9900/builtins/build/libbuiltins.a

# Softfloat library
SOFTFLOAT_DIR = $(ROOT)/libtms9900/builtins/softfloat
SOFTFLOAT_SRCS = addsf3.c subsf3.c mulsf3.c divsf3.c negsf2.c \
                 comparesf2.c floatsisf.c floatunsisf.c fixsfsi.c \
                 fixunssfsi.c clzsi2.c
SOFTFLOAT_OBJS = $(SOFTFLOAT_SRCS:%.c=$(BUILD)/sf_%.o)

# Softfloat support (64-bit builtins, FP env stubs)
FP_BUILTINS_SRC = $(ROOT)/tests/fp32_builtins.c

# Benchmark programs (no special libs needed)
BASIC_BENCHMARKS = fib bubble_sort deep_recursion crc32 q7_8_matmul json_parse vertex3d huffman long_torture heap4

# Benchmarks needing strlen/strcmp (link with builtins for memcpy etc)
STRING_BENCHMARKS = string_torture

# Benchmarks needing soft-float
FLOAT_BENCHMARKS = float_torture

# Benchmarks needing builtins (memcmp, popcount, bswap, clz, ctz)
BITOP_BENCHMARKS = bitops_torture

ALL_BENCHMARKS = $(BASIC_BENCHMARKS) $(STRING_BENCHMARKS) $(FLOAT_BENCHMARKS) $(BITOP_BENCHMARKS)

# Default: build all
all: $(ALL_BENCHMARKS:%=$(BUILD)/%.bin)

$(BUILD):
	mkdir -p $(BUILD)

# Startup object
$(BUILD)/bench_start.o: bench_start.s | $(BUILD)
	$(CC) $(TARGET) -c -o $@ $<

# Pattern rule: compile benchmark C -> .o
$(BUILD)/%.o: %.c | $(BUILD)
	$(CC) $(CFLAGS) -c -o $@ $<

# Softfloat objects (always -O2 — runtime library is precompiled)
$(BUILD)/sf_%.o: $(SOFTFLOAT_DIR)/%.c | $(BUILD)
	$(CC) $(SF_CFLAGS) -c -o $@ $<

# FP builtins (always -O2 — runtime library is precompiled)
$(BUILD)/fp32_builtins.o: $(FP_BUILTINS_SRC) | $(BUILD)
	$(CC) $(SF_CFLAGS) -c -o $@ $<

# Basic benchmarks: startup + benchmark + builtins
$(BUILD)/fib.elf $(BUILD)/bubble_sort.elf $(BUILD)/deep_recursion.elf \
$(BUILD)/crc32.elf $(BUILD)/q7_8_matmul.elf $(BUILD)/json_parse.elf \
$(BUILD)/vertex3d.elf $(BUILD)/huffman.elf $(BUILD)/long_torture.elf \
$(BUILD)/heap4.elf: \
$(BUILD)/%.elf: $(BUILD)/bench_start.o $(BUILD)/%.o $(BUILTINS) | $(BUILD)
	$(LD) $(LDFLAGS) -o $@ $^

# String benchmarks: need builtins for strlen/strcmp
$(BUILD)/string_torture.elf: $(BUILD)/bench_start.o $(BUILD)/string_torture.o $(BUILTINS) | $(BUILD)
	$(LD) $(LDFLAGS) -o $@ $^

# Bitops benchmarks: need builtins for memcmp etc.
$(BUILD)/bitops_torture.elf: $(BUILD)/bench_start.o $(BUILD)/bitops_torture.o $(BUILTINS) | $(BUILD)
	$(LD) $(LDFLAGS) -o $@ $^

# Float benchmarks: need softfloat + fp builtins + builtins
$(BUILD)/float_torture.elf: $(BUILD)/bench_start.o $(BUILD)/float_torture.o $(BUILD)/fp32_builtins.o $(SOFTFLOAT_OBJS) $(BUILTINS) | $(BUILD)
	$(LD) $(LDFLAGS) -o $@ $^

# ELF -> binary
$(BUILD)/%.bin: $(BUILD)/%.elf
	$(OBJCOPY) -O binary \
		--only-section=.text \
		--only-section=.rodata \
		--only-section=.data \
		$< $@
	@$(SIZE) $<

# Convenience targets for individual benchmarks
$(ALL_BENCHMARKS): %: $(BUILD)/%.bin
	@echo "Built $@"

clean:
	rm -rf build

info:
	@echo "Benchmarks: $(ALL_BENCHMARKS)"
	@echo "Optimization: -O$(OPT)"
	@echo "Build dir: $(BUILD)"

.PHONY: all clean info $(ALL_BENCHMARKS)
