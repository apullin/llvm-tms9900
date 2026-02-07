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
PYTHON = python3
BIN2RPK = ../bin2rpk.py
BIN2EA5 = ../bin2ea5.py

# Flags
CFLAGS = --target=tms9900 -O3 -fno-builtin -ffreestanding -Wno-main-return-type
LDFLAGS = -T cart.ld --print-memory-usage
EA5_LDFLAGS = -T ea5.ld --print-memory-usage
EA5_BASE = 0xA000
EA5_MAX_SIZE = 0x6000

PROGRAMS = banner ball ball2 life life2 life2x life2_2x kb_test irq_test
EA5_PROGRAMS = $(PROGRAMS)

.PHONY: all clean

all: $(PROGRAMS:%=%.bin) $(PROGRAMS:%=%.cart) $(PROGRAMS:%=%.rpk) $(EA5_PROGRAMS:%=%.img)

# Pattern rules
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

%.o: %.S
	$(CC) --target=tms9900 -c $< -o $@

%.elf: %.o cart.ld
	$(LD) $(LDFLAGS) $< -o $@
	$(OBJDUMP) -d $@ > $(@:.elf=.list)

%.ea5.elf: %.o ea5.ld
	$(LD) $(EA5_LDFLAGS) $< -o $@
	$(OBJDUMP) -d $@ > $(@:.elf=.list)

%.ea5.bin: %.ea5.elf
	$(OBJCOPY) -O binary --only-section=.text --only-section=.rodata --only-section=.data $< $@

%.bin: %.elf
	$(OBJCOPY) -O binary --only-section=.cart_header --only-section=.cart_entry --only-section=.text --only-section=.rodata --only-section=.data $< $@

%.cart: %.bin
	cp $< $@

# RPK only applies to cartridge .bin images.
$(PROGRAMS:%=%.rpk): %.rpk: %.bin
	$(PYTHON) $(BIN2RPK) $< -o $@

%.img: %.ea5.bin
	$(PYTHON) $(BIN2EA5) --max-size $(EA5_MAX_SIZE) -a $(EA5_BASE) $< -o $@

%.list: %.elf
	$(OBJDUMP) -d $< > $@

# Assemble crt0
crt0.o: crt0.s
	$(CC) --target=tms9900 -c $< -o $@

# Programs that use crt0 for startup (BSS/data init)
ball.elf: crt0.o ball.o cart.ld
	$(LD) $(LDFLAGS) crt0.o ball.o -o $@
	$(OBJDUMP) -d $@ > $(@:.elf=.list)

ball.ea5.elf: crt0.o ball.o ea5.ld
	$(LD) $(EA5_LDFLAGS) crt0.o ball.o -o $@
	$(OBJDUMP) -d $@ > $(@:.elf=.list)

ball2.elf: crt0.o ball2.o cart.ld
	$(LD) $(LDFLAGS) crt0.o ball2.o -o $@
	$(OBJDUMP) -d $@ > $(@:.elf=.list)

ball2.ea5.elf: crt0.o ball2.o ea5.ld
	$(LD) $(EA5_LDFLAGS) crt0.o ball2.o -o $@
	$(OBJDUMP) -d $@ > $(@:.elf=.list)

life.elf: crt0.o life.o cart.ld
	$(LD) $(LDFLAGS) crt0.o life.o -o $@
	$(OBJDUMP) -d $@ > $(@:.elf=.list)

life.ea5.elf: crt0.o life.o ea5.ld
	$(LD) $(EA5_LDFLAGS) crt0.o life.o -o $@
	$(OBJDUMP) -d $@ > $(@:.elf=.list)

life2.elf: crt0.o life2.o cart.ld
	$(LD) $(LDFLAGS) crt0.o life2.o -o $@
	$(OBJDUMP) -d $@ > $(@:.elf=.list)

life2.ea5.elf: crt0.o life2.o ea5.ld
	$(LD) $(EA5_LDFLAGS) crt0.o life2.o -o $@
	$(OBJDUMP) -d $@ > $(@:.elf=.list)

life2x.elf: crt0.o life2x.o kbd.o cart.ld
	$(LD) $(LDFLAGS) crt0.o life2x.o kbd.o -o $@
	$(OBJDUMP) -d $@ > $(@:.elf=.list)

life2x.ea5.elf: crt0.o life2x.o kbd.o ea5.ld
	$(LD) $(EA5_LDFLAGS) crt0.o life2x.o kbd.o -o $@
	$(OBJDUMP) -d $@ > $(@:.elf=.list)

life2_2x.elf: crt0.o life2_2x.o kbd.o cart.ld
	$(LD) $(LDFLAGS) crt0.o life2_2x.o kbd.o -o $@
	$(OBJDUMP) -d $@ > $(@:.elf=.list)

life2_2x.ea5.elf: crt0.o life2_2x.o kbd.o ea5.ld
	$(LD) $(EA5_LDFLAGS) crt0.o life2_2x.o kbd.o -o $@
	$(OBJDUMP) -d $@ > $(@:.elf=.list)

kb_test.elf: crt0.o kb_test.o kbd.o cart.ld
	$(LD) $(LDFLAGS) crt0.o kb_test.o kbd.o -o $@
	$(OBJDUMP) -d $@ > $(@:.elf=.list)

kb_test.ea5.elf: crt0.o kb_test.o kbd.o ea5.ld
	$(LD) $(EA5_LDFLAGS) crt0.o kb_test.o kbd.o -o $@
	$(OBJDUMP) -d $@ > $(@:.elf=.list)

irq_test.elf: crt0.o irq_test.o cart.ld
	$(LD) $(LDFLAGS) crt0.o irq_test.o -o $@
	$(OBJDUMP) -d $@ > $(@:.elf=.list)

irq_test.ea5.elf: crt0.o irq_test.o ea5.ld
	$(LD) $(EA5_LDFLAGS) crt0.o irq_test.o -o $@
	$(OBJDUMP) -d $@ > $(@:.elf=.list)

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
	rm -f *.o *.elf *.bin *.cart *.rpk *.list *.ea5.elf *.ea5.bin *.img
