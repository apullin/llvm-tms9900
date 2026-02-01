# libm for TMS9900
# Combines our optimized functions with picolibc generic implementations

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

# Our optimized implementations (override picolibc)
OUR_SRCS = sincosf_tiny.c

# Picolibc math functions (NOT sin/cos - we override those)
PICO_MATH_SRCS = \
    sf_sqrt.c sf_fabs.c sf_floor.c sf_ceil.c \
    sf_exp.c sf_exp2.c sf_log.c sf_log10.c sf_log2.c \
    sf_pow.c sf_fmod.c sf_frexp.c \
    sf_atan.c sf_atan2.c sf_asin.c sf_acos.c \
    sf_sinh.c sf_cosh.c sf_tanh.c \
    sf_asinh.c sf_acosh.c sf_atanh.c \
    sf_hypot.c \
    sf_tan.c sf_rem_pio2.c kf_rem_pio2.c kf_tan.c

# Picolibc common functions
PICO_COMMON_SRCS = \
    sf_scalbn.c sf_copysign.c sf_finite.c sf_isnan.c sf_isinf.c sf_issignaling.c sf_modf.c

OUR_OBJS = $(OUR_SRCS:%.c=$(BUILD)/%.o)
PICO_MATH_OBJS = $(PICO_MATH_SRCS:%.c=$(BUILD)/%.o)
PICO_COMMON_OBJS = $(PICO_COMMON_SRCS:%.c=$(BUILD)/%.o)

ALL_OBJS = $(OUR_OBJS) $(PICO_MATH_OBJS) $(PICO_COMMON_OBJS)

.PHONY: all clean sizes

all: $(BUILD)/libm.a sizes

$(BUILD):
	mkdir -p $(BUILD)

# Our optimized functions
$(BUILD)/%.o: %.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

# Picolibc math functions
$(BUILD)/sf_sqrt.o: $(PICO_MATH)/sf_sqrt.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@
$(BUILD)/sf_fabs.o: $(PICO_MATH)/sf_fabs.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@
$(BUILD)/sf_floor.o: $(PICO_MATH)/sf_floor.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@
$(BUILD)/sf_ceil.o: $(PICO_MATH)/sf_ceil.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@
$(BUILD)/sf_exp.o: $(PICO_MATH)/sf_exp.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@
$(BUILD)/sf_exp2.o: $(PICO_MATH)/sf_exp2.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@
$(BUILD)/sf_log.o: $(PICO_MATH)/sf_log.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@
$(BUILD)/sf_log10.o: $(PICO_MATH)/sf_log10.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@
$(BUILD)/sf_log2.o: $(PICO_MATH)/sf_log2.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@
$(BUILD)/sf_pow.o: $(PICO_MATH)/sf_pow.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@
$(BUILD)/sf_fmod.o: $(PICO_MATH)/sf_fmod.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@
$(BUILD)/sf_frexp.o: $(PICO_MATH)/sf_frexp.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@
$(BUILD)/sf_atan.o: $(PICO_MATH)/sf_atan.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@
$(BUILD)/sf_atan2.o: $(PICO_MATH)/sf_atan2.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@
$(BUILD)/sf_asin.o: $(PICO_MATH)/sf_asin.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@
$(BUILD)/sf_acos.o: $(PICO_MATH)/sf_acos.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@
$(BUILD)/sf_sinh.o: $(PICO_MATH)/sf_sinh.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@
$(BUILD)/sf_cosh.o: $(PICO_MATH)/sf_cosh.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@
$(BUILD)/sf_tanh.o: $(PICO_MATH)/sf_tanh.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@
$(BUILD)/sf_asinh.o: $(PICO_MATH)/sf_asinh.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@
$(BUILD)/sf_acosh.o: $(PICO_MATH)/sf_acosh.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@
$(BUILD)/sf_atanh.o: $(PICO_MATH)/sf_atanh.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@
$(BUILD)/sf_hypot.o: $(PICO_MATH)/sf_hypot.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@
$(BUILD)/sf_tan.o: $(PICO_MATH)/sf_tan.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@
$(BUILD)/sf_rem_pio2.o: $(PICO_MATH)/sf_rem_pio2.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@
$(BUILD)/kf_rem_pio2.o: $(PICO_MATH)/kf_rem_pio2.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@
$(BUILD)/kf_tan.o: $(PICO_MATH)/kf_tan.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

# Picolibc common functions
$(BUILD)/sf_scalbn.o: $(PICO_COMMON)/sf_scalbn.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@
$(BUILD)/sf_copysign.o: $(PICO_COMMON)/sf_copysign.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@
$(BUILD)/sf_finite.o: $(PICO_COMMON)/sf_finite.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@
$(BUILD)/sf_isnan.o: $(PICO_COMMON)/sf_isnan.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@
$(BUILD)/sf_isinf.o: $(PICO_COMMON)/sf_isinf.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@
$(BUILD)/sf_issignaling.o: $(PICO_COMMON)/sf_issignaling.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@
$(BUILD)/sf_modf.o: $(PICO_COMMON)/sf_modf.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/libm.a: $(ALL_OBJS)
	$(AR) rcs $@ $(ALL_OBJS)

sizes: $(BUILD)/libm.a
	@echo ""
	@echo "=== libm.a for TMS9900 ==="
	@$(SIZE) -t $(ALL_OBJS) 2>/dev/null | tail -1
	@echo ""
	@ls -la $(BUILD)/libm.a

clean:
	rm -rf $(BUILD)
