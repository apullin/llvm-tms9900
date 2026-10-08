# libm for TMS9900
# Builds picolibc's binary32 implementations, including full-range sin/cos.

CC = ../../llvm-project/build/bin/clang
AR = ../../llvm-project/build/bin/llvm-ar
SIZE = ../../llvm-project/build/bin/llvm-size

CFLAGS = --target=tms9900 -Os -ffreestanding -fno-builtin \
         -I ../picolibc/libc/include \
         -I ../picolibc/libm/common \
         -I include \
         -include include/picolibc.h \
         -D_COMPILING_NEWLIB

PICO_MATH = ../picolibc/libm/math
PICO_COMMON = ../picolibc/libm/common

BUILD = build

# Picolibc math functions. Use the full-range reducer and compensated kernels
# for sin/cos, as for tan; the former compact override did not meet libm's
# binary32 domain or accuracy requirements.
PICO_MATH_SRCS = \
    sf_sqrt.c sf_fabs.c sf_floor.c sf_ceil.c \
    sf_exp.c sf_exp2.c sf_log.c sf_log10.c sf_log2.c \
    sf_pow.c sf_fmod.c sf_frexp.c \
    sf_atan.c sf_atan2.c sf_asin.c sf_acos.c \
    sf_sinh.c sf_cosh.c sf_tanh.c \
    sf_asinh.c sf_acosh.c sf_atanh.c \
    sf_hypot.c \
    sf_sin.c sf_cos.c kf_sin.c kf_cos.c \
    sf_tan.c sf_rem_pio2.c kf_rem_pio2.c kf_tan.c

# Picolibc common functions
PICO_COMMON_SRCS = \
    sf_scalbn.c sf_copysign.c sf_finite.c sf_isnan.c sf_isinf.c sf_issignaling.c sf_modf.c \
    sf_fpclassify.c sf_round.c \
    math_errf_invalidf.c math_errf_oflowf.c math_errf_uflowf.c math_errf_divzerof.c

PICO_MATH_OBJS = $(PICO_MATH_SRCS:%.c=$(BUILD)/%.o)
PICO_COMMON_OBJS = $(PICO_COMMON_SRCS:%.c=$(BUILD)/%.o)

ALL_OBJS = $(PICO_MATH_OBJS) $(PICO_COMMON_OBJS)

.PHONY: all clean sizes

all: $(BUILD)/libm.a sizes

$(BUILD):
	mkdir -p $(BUILD)

# Header dependencies include picolibc's configuration and math internals.
$(PICO_MATH_OBJS): $(BUILD)/%.o: $(PICO_MATH)/%.c | $(BUILD)
	$(CC) $(CFLAGS) -MMD -MP -MF $(@:.o=.d) -c $< -o $@

$(PICO_COMMON_OBJS): $(BUILD)/%.o: $(PICO_COMMON)/%.c | $(BUILD)
	$(CC) $(CFLAGS) -MMD -MP -MF $(@:.o=.d) -c $< -o $@

$(BUILD)/libm.a: $(ALL_OBJS) Makefile
	# Recreate the archive: ar rcs alone would leave the old sin/cos override
	# member behind when upgrading an existing build directory.
	rm -f $@.tmp
	$(AR) rcs $@.tmp $(ALL_OBJS)
	mv $@.tmp $@

sizes: $(BUILD)/libm.a
	@echo ""
	@echo "=== libm.a for TMS9900 ==="
	@$(SIZE) -t $(ALL_OBJS) 2>/dev/null | tail -1
	@echo ""
	@ls -la $(BUILD)/libm.a

clean:
	rm -rf $(BUILD)

CONFIG_STAMP = $(BUILD)/.build-config.json
CONFIG_ARGS = --value=$(call quote_config,CFLAGS=$(CFLAGS)) \
              --tool=$(call quote_config,$(CC)) --tool=$(call quote_config,$(AR))
CONFIG_OUTPUTS = $(ALL_OBJS) $(BUILD)/libm.a
CONFIG_PATTERNS = *.o *.d *.a *.a.tmp
include ../../tools/build_config.mk
-include $(wildcard $(BUILD)/*.d)
