# FreeRTOS TMS9900 port - Makefile
#
# Build:  make                    (builds both test programs)
# Clean:  make clean
# Test:   make run                (basic two-task test)
#         make run-queue          (queue + rendezvous test)

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
             -I FreeRTOS-Kernel/portable/GCC/TMS9900
ASFLAGS   := $(TARGET) -c
LDFLAGS   := -T linker.ld

BUILDDIR := build

# FreeRTOS kernel sources (minimal set)
KERNEL_SRC := \
    FreeRTOS-Kernel/tasks.c \
    FreeRTOS-Kernel/list.c \
    FreeRTOS-Kernel/queue.c

# Extended kernel sources (for queue demo)
KERNEL_EXT_SRC := \
    FreeRTOS-Kernel/event_groups.c

# Port sources
PORT_SRC := \
    FreeRTOS-Kernel/portable/GCC/TMS9900/port.c \
    FreeRTOS-Kernel/portable/GCC/TMS9900/portasm.S

# Shared support sources
SUPPORT_SRC := \
    minilib.c \
    builtins.S \
    startup.S

# Object files for shared components
KERNEL_OBJS  := $(patsubst %.c,$(BUILDDIR)/%.o,$(KERNEL_SRC))
KERNEL_EXT_OBJS := $(patsubst %.c,$(BUILDDIR)/%.o,$(KERNEL_EXT_SRC))
PORT_OBJS    := $(patsubst %.c,$(BUILDDIR)/%.o,$(patsubst %.S,$(BUILDDIR)/%.o,$(PORT_SRC)))
SUPPORT_OBJS := $(patsubst %.c,$(BUILDDIR)/%.o,$(patsubst %.S,$(BUILDDIR)/%.o,$(SUPPORT_SRC)))

# Test 1: basic two-task test
TEST1_ELF  := $(BUILDDIR)/freertos_test.elf
TEST1_BIN  := $(BUILDDIR)/freertos_test.bin
TEST1_LIST := $(BUILDDIR)/freertos_test.list
TEST1_OBJS := $(KERNEL_OBJS) $(PORT_OBJS) $(BUILDDIR)/main.o $(SUPPORT_OBJS)

# Test 2: queue + rendezvous test
TEST2_ELF  := $(BUILDDIR)/freertos_queue.elf
TEST2_BIN  := $(BUILDDIR)/freertos_queue.bin
TEST2_LIST := $(BUILDDIR)/freertos_queue.list
TEST2_OBJS := $(KERNEL_OBJS) $(KERNEL_EXT_OBJS) $(PORT_OBJS) $(BUILDDIR)/main_queue.o $(SUPPORT_OBJS)

.PHONY: all clean run run-queue

all: $(TEST1_BIN) $(TEST1_LIST) $(TEST2_BIN) $(TEST2_LIST)

# Binary extraction
$(BUILDDIR)/%.bin: $(BUILDDIR)/%.elf
	$(OBJCOPY) -O binary $< $@

$(BUILDDIR)/%.list: $(BUILDDIR)/%.elf
	$(OBJDUMP) -d $< > $@

# Link test 1
$(TEST1_ELF): $(TEST1_OBJS) linker.ld
	$(LD) $(LDFLAGS) $(TEST1_OBJS) -o $@

# Link test 2
$(TEST2_ELF): $(TEST2_OBJS) linker.ld
	$(LD) $(LDFLAGS) $(TEST2_OBJS) -o $@

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

# Run basic two-task test
run: $(TEST1_BIN)
	$(TRACE) \
	    --timer=1:50000 \
	    -l 0x0000 -e 0x0100 -w 0x8300 \
	    -n 500000 \
	    -d 0x7F00:8 \
	    $(TEST1_BIN)

# Run queue + rendezvous test
run-queue: $(TEST2_BIN)
	$(TRACE) \
	    --timer=1:50000 \
	    -l 0x0000 -e 0x0100 -w 0x8300 \
	    -n 1000000 \
	    -d 0x7F00:8 \
	    $(TEST2_BIN)
