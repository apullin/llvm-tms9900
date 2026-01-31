# FreeRTOS TMS9900 port - Makefile
#
# Build:  make
# Clean:  make clean
# Test:   make run

LLVM_BIN  := ../llvm-project/build/bin
CC        := $(LLVM_BIN)/clang
AS        := $(CC)
LD        := $(LLVM_BIN)/ld.lld
OBJCOPY   := $(LLVM_BIN)/llvm-objcopy
OBJDUMP   := $(LLVM_BIN)/llvm-objdump
TRACE     := ../../tms9900-trace/build/tms9900-trace

TARGET    := --target=tms9900
CFLAGS    := $(TARGET) -Os -ffreestanding -nostdlibinc -fno-builtin \
             -isystem include \
             -I . \
             -I FreeRTOS-Kernel/include \
             -I portable/GCC/TMS9900
ASFLAGS   := $(TARGET) -c
LDFLAGS   := -T linker.ld

# FreeRTOS kernel sources (minimal set)
KERNEL_SRC := \
    FreeRTOS-Kernel/tasks.c \
    FreeRTOS-Kernel/list.c \
    FreeRTOS-Kernel/queue.c

# Port sources
PORT_SRC := \
    portable/GCC/TMS9900/port.c \
    portable/GCC/TMS9900/portasm.S

# Application sources
APP_SRC := \
    main.c \
    minilib.c \
    builtins.S \
    startup.S

# All sources
ALL_SRC := $(KERNEL_SRC) $(PORT_SRC) $(APP_SRC)

# Object files (in build/ directory)
BUILDDIR := build
OBJS := $(patsubst %.c,$(BUILDDIR)/%.o,$(patsubst %.S,$(BUILDDIR)/%.o,$(ALL_SRC)))

# Output
ELF  := $(BUILDDIR)/freertos_test.elf
BIN  := $(BUILDDIR)/freertos_test.bin
LIST := $(BUILDDIR)/freertos_test.list

.PHONY: all clean run

all: $(BIN) $(LIST)

$(BIN): $(ELF)
	$(OBJCOPY) -O binary $< $@

$(LIST): $(ELF)
	$(OBJDUMP) -d $< > $@

$(ELF): $(OBJS) linker.ld
	$(LD) $(LDFLAGS) $(OBJS) -o $@

# C source compilation
$(BUILDDIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

# Assembly compilation
$(BUILDDIR)/%.o: %.S
	@mkdir -p $(dir $@)
	$(AS) $(ASFLAGS) $< -o $@

clean:
	rm -rf $(BUILDDIR)

# Run test in tms9900-trace with periodic tick timer
run: $(BIN)
	$(TRACE) \
	    --timer=1:50000 \
	    -l 0x0000 -e 0x0100 -w 0x8300 \
	    -n 500000 \
	    -d 0x7F00:8 \
	    $(BIN)
