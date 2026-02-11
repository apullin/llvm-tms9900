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

# C++ flags (freestanding, no exceptions/RTTI)
CXXFLAGS = $(TARGET) -O$(OPT) -ffreestanding -fno-builtin -nostdlib -fno-exceptions -fno-rtti -fno-threadsafe-statics -nostdinc++ $(EXTRA_CFLAGS)

# C++ flags with libc++ STL headers (header-only freestanding subset)
STLFLAGS = $(CXXFLAGS) -isystem libcxx_config -isystem $(ROOT)/llvm-project/libcxx/include

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
BASIC_BENCHMARKS = fib bubble_sort deep_recursion crc32 q7_8_matmul json_parse vertex3d huffman long_torture heap4 i64_torture

# Benchmarks needing strlen/strcmp (link with builtins for memcpy etc)
STRING_BENCHMARKS = string_torture

# Benchmarks needing soft-float
FLOAT_BENCHMARKS = float_torture

# Benchmarks needing builtins (memcmp, popcount, bswap, clz, ctz)
BITOP_BENCHMARKS = bitops_torture

# C++ benchmarks
CPP_BENCHMARKS = cpp_test

# C++ feature test benchmarks (need cxxrt + builtins)
CPP_FEATURE_BENCHMARKS = lambda_test mi_test cpp_adv_test

# C++ STL benchmarks (libc++ header-only)
STL_BENCHMARKS = stl_test stl_util_test

ALL_BENCHMARKS = $(BASIC_BENCHMARKS) $(STRING_BENCHMARKS) $(FLOAT_BENCHMARKS) $(BITOP_BENCHMARKS) $(CPP_BENCHMARKS) $(CPP_FEATURE_BENCHMARKS) $(STL_BENCHMARKS)

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

# Pattern rule: compile C++ -> .o
$(BUILD)/%.o: %.cpp | $(BUILD)
	$(CC) $(CXXFLAGS) -c -o $@ $<

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
$(BUILD)/heap4.elf $(BUILD)/i64_torture.elf: \
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

# C++ benchmarks: need cxxrt + builtins
$(BUILD)/cpp_test.elf: $(BUILD)/bench_start.o $(BUILD)/cpp_test.o $(BUILD)/cxxrt.o $(BUILTINS) | $(BUILD)
	$(LD) $(LDFLAGS) -o $@ $^

# STL tests: compile with libc++ headers
$(BUILD)/stl_test.o: stl_test.cpp | $(BUILD)
	$(CC) $(STLFLAGS) -c -o $@ $<

$(BUILD)/stl_util_test.o: stl_util_test.cpp | $(BUILD)
	$(CC) $(STLFLAGS) -c -o $@ $<

# C++ feature tests: compile with appropriate flags
$(BUILD)/lambda_test.o: lambda_test.cpp | $(BUILD)
	$(CC) $(STLFLAGS) -c -o $@ $<

$(BUILD)/cpp_adv_test.o: cpp_adv_test.cpp | $(BUILD)
	$(CC) $(STLFLAGS) -c -o $@ $<

# C++ feature test benchmarks: need cxxrt + builtins
$(BUILD)/lambda_test.elf $(BUILD)/mi_test.elf $(BUILD)/cpp_adv_test.elf: \
$(BUILD)/%.elf: $(BUILD)/bench_start.o $(BUILD)/%.o $(BUILD)/cxxrt.o $(BUILTINS) | $(BUILD)
	$(LD) $(LDFLAGS) -o $@ $^

# STL benchmarks: need cxxrt + builtins
$(BUILD)/stl_test.elf: $(BUILD)/bench_start.o $(BUILD)/stl_test.o $(BUILD)/cxxrt.o $(BUILTINS) | $(BUILD)
	$(LD) $(LDFLAGS) -o $@ $^

$(BUILD)/stl_util_test.elf: $(BUILD)/bench_start.o $(BUILD)/stl_util_test.o $(BUILD)/cxxrt.o $(BUILTINS) | $(BUILD)
	$(LD) $(LDFLAGS) -o $@ $^

# ELF -> binary
$(BUILD)/%.bin: $(BUILD)/%.elf
	$(OBJCOPY) -O binary \
		--only-section=.text \
		--only-section=.rodata \
		--only-section=.init_array \
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
