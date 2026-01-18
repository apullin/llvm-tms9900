# TI-99/4A Cartridge Build
#
# Produces .bin ROM images (0x6000-0x7FFF) and .cart copies for js99er.

# LLVM toolchain
LLVM_BUILD = ../llvm-project/build/bin
CC = $(LLVM_BUILD)/clang
LD = $(LLVM_BUILD)/ld.lld
OBJCOPY = $(LLVM_BUILD)/llvm-objcopy
OBJDUMP = $(LLVM_BUILD)/llvm-objdump
READELF = $(LLVM_BUILD)/llvm-readelf

# Flags
CFLAGS = --target=tms9900 -O2 -fno-builtin -ffreestanding -Wno-main-return-type
LDFLAGS = -T cart.ld --print-memory-usage

PROGRAMS = banner ball ball2

.PHONY: all clean

all: $(PROGRAMS:%=%.bin) $(PROGRAMS:%=%.cart)

# Pattern rules
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

%.o: %.S
	$(CC) --target=tms9900 -c $< -o $@

%.elf: %.o cart.ld
	$(LD) $(LDFLAGS) $< -o $@

%.bin: %.elf
	$(OBJCOPY) -O binary --only-section=.cart_header --only-section=.cart_entry --only-section=.text --only-section=.rodata --only-section=.data $< $@

%.cart: %.bin
	cp $< $@

%.list: %.elf
	$(OBJDUMP) -d $< > $@

# Assemble crt0
crt0.o: crt0.s
	$(CC) --target=tms9900 -c $< -o $@

# Programs that use crt0 for startup (BSS/data init)
ball.elf: crt0.o ball.o cart.ld
	$(LD) $(LDFLAGS) crt0.o ball.o -o $@

ball2.elf: crt0.o ball2.o cart.ld
	$(LD) $(LDFLAGS) crt0.o ball2.o -o $@

dump-%: %.elf
	@echo "=== Sections ==="
	$(READELF) -S $<
	@echo ""
	@echo "=== Symbols ==="
	$(READELF) -s $<
	@echo ""
	@echo "=== Disassembly ==="
	$(OBJDUMP) -d $<

hexdump-%: %.bin
	xxd $< | head -20

clean:
	rm -f *.o *.elf *.bin *.cart *.list
