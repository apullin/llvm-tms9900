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
AR       = $(LLVM_BIN)/llvm-ar
OBJCOPY  = $(LLVM_BIN)/llvm-objcopy
SIZE     = $(LLVM_BIN)/llvm-size

TARGET   = --target=tms9900
OPT      ?= 2
LTO      ?= 0
BUILTINS_MODE ?= prebuilt
# -mno-relax-all needed at -O0 to prevent "fixup value out of range" on large functions
ifeq ($(OPT),0)
EXTRA_CFLAGS = -mno-relax-all
endif
ifeq ($(LTO),1)
LTO_CFLAGS = -flto -flto-linker-scripts
LTO_SUFFIX = -lto
LTO_OPT = $(if $(filter s z,$(OPT)),2,$(OPT))
LTO_LDFLAGS = --lto-O$(LTO_OPT) --lto-linker-scripts
endif

# C++ flags (freestanding, no exceptions/RTTI)
CXXFLAGS = $(TARGET) -O$(OPT) -ffreestanding -fno-builtin -nostdlib -fno-exceptions -fno-rtti -fno-threadsafe-statics -nostdinc++ $(EXTRA_CFLAGS) $(LTO_CFLAGS)

# C++ flags with libc++ STL headers (header-only freestanding subset)
STLFLAGS = $(CXXFLAGS) -isystem libcxx_config -isystem $(ROOT)/llvm-project/libcxx/include

# Softfloat/runtime always compiled at -O2 (matches real-world precompiled library)
SF_CFLAGS = $(TARGET) -O2 -ffreestanding -fno-builtin -nostdlib $(LTO_CFLAGS)

ifeq ($(BUILTINS_MODE),lto-archive)
BUILTINS_SUFFIX = -blto-gc
BUILTINS_BUILD_DIR ?= build-lto-gc-O$(OPT)
BUILTINS_LTO_CFLAGS = $(TARGET) -O$(OPT) -fno-builtin -c -flto -flto-linker-scripts -ffunction-sections -fdata-sections
BUILTINS_I64_OPT = $(if $(filter s z,$(OPT)),2,$(OPT))
BUILTINS_I64_LTO_CFLAGS = $(TARGET) -O$(BUILTINS_I64_OPT) -fno-builtin -c -flto -flto-linker-scripts -ffunction-sections -fdata-sections
BUILTINS_LDFLAGS = --gc-sections
else ifeq ($(BUILTINS_MODE),local)
BUILTINS_SUFFIX = -local
endif

BUILD ?= build/O$(OPT)$(LTO_SUFFIX)$(BUILTINS_SUFFIX)
CFLAGS   = $(TARGET) -O$(OPT) -ffreestanding -fno-builtin -nostdlib $(EXTRA_CFLAGS) $(LTO_CFLAGS)
LDFLAGS  = -T bench.ld $(LTO_LDFLAGS) $(BUILTINS_LDFLAGS)

BUILTINS_DIR = $(ROOT)/libtms9900/builtins

ifeq ($(BUILTINS_MODE),lto-archive)
BUILTINS ?= $(BUILTINS_DIR)/$(BUILTINS_BUILD_DIR)/libbuiltins.a
else ifeq ($(BUILTINS_MODE),local)
BUILTINS ?= $(LOCAL_BUILTINS_I64_OBJS) $(LOCAL_BUILTINS_ARITH_C_OBJS) \
            $(BUILD)/libbuiltins_rt.a $(BUILD)/libbuiltins_asm.a
else
BUILTINS ?= $(BUILTINS_DIR)/build/libbuiltins.a
endif

# Builtins sources for local build mode
BUILTINS_ASM_SRCS = mul32.S div32.S shift32.S memcpy.S memset.S memmove.S \
                    bswapsi2.S clzsi2.S ctzsi2.S ashldi3.S negdi2.S \
                    cmpdi2.S clzdi2.S ctzdi2.S ffssi2.S paritysi2.S \
                    popcountsi2.S bswapdi2.S absvsi2.S absvdi2.S \
                    addvsi3.S addvdi3.S mulvsi3.S divmodsi4.S
BUILTINS_I64_SRCS = i64/muldi3.c i64/udivmoddi4.c i64/divdi3.c i64/udivdi3.c \
                    i64/moddi3.c i64/umoddi3.c
BUILTINS_ARITH_C_SRCS = mulvdi3.c mulosi4.c
BUILTINS_RT_C_SRCS = malloc.c string_utils.c

LOCAL_BUILTINS_ASM_OBJS = $(patsubst %.S,$(BUILD)/builtins/%.o,$(BUILTINS_ASM_SRCS))
LOCAL_BUILTINS_I64_OBJS = $(patsubst %.c,$(BUILD)/builtins/%.o,$(BUILTINS_I64_SRCS))
LOCAL_BUILTINS_ARITH_C_OBJS = $(patsubst %.c,$(BUILD)/builtins/%.o,$(BUILTINS_ARITH_C_SRCS))
LOCAL_BUILTINS_RT_C_OBJS = $(patsubst %.c,$(BUILD)/builtins/%.o,$(BUILTINS_RT_C_SRCS))
LOCAL_BUILTINS_CFLAGS = $(TARGET) -O$(OPT) -ffreestanding -fno-builtin -nostdlib $(EXTRA_CFLAGS) $(LTO_CFLAGS)

# Softfloat library
SOFTFLOAT_DIR = $(ROOT)/libtms9900/builtins/softfloat
SOFTFLOAT_SRCS = addsf3.c subsf3.c mulsf3.c divsf3.c negsf2.c \
                 comparesf2.c floatsisf.c floatunsisf.c fixsfsi.c \
                 fixunssfsi.c
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

ifeq ($(BUILTINS_MODE),lto-archive)
$(BUILTINS_DIR)/$(BUILTINS_BUILD_DIR)/libbuiltins.a:
	$(MAKE) -C $(BUILTINS_DIR) BUILD=$(BUILTINS_BUILD_DIR) \
		CFLAGS="$(BUILTINS_LTO_CFLAGS)" \
		I64_CFLAGS="$(BUILTINS_I64_LTO_CFLAGS)" \
		all
endif

$(BUILD)/libbuiltins_asm.a: $(LOCAL_BUILTINS_ASM_OBJS) | $(BUILD)
	$(AR) rcs $@ $^

$(BUILD)/libbuiltins_rt.a: $(LOCAL_BUILTINS_RT_C_OBJS) | $(BUILD)
	$(AR) rcs $@ $^

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

# Local builtins build mode: compile builtins into the benchmark build tree
$(BUILD)/builtins/%.o: $(BUILTINS_DIR)/%.S | $(BUILD)
	@mkdir -p $(dir $@)
	$(CC) $(TARGET) -c -o $@ $<

$(BUILD)/builtins/%.o: $(BUILTINS_DIR)/i64/%.c | $(BUILD)
	@mkdir -p $(dir $@)
	$(CC) $(LOCAL_BUILTINS_CFLAGS) -c -o $@ $<

$(BUILD)/builtins/%.o: $(BUILTINS_DIR)/%.c | $(BUILD)
	@mkdir -p $(dir $@)
	$(CC) $(LOCAL_BUILTINS_CFLAGS) -c -o $@ $<

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
	@echo "LTO: $(LTO)"
	@echo "Builtins mode: $(BUILTINS_MODE)"
	@echo "Builtins: $(BUILTINS)"
	@echo "Build dir: $(BUILD)"

.PHONY: all clean info $(ALL_BENCHMARKS)
