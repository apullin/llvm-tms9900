# TI-99/4A demos: flat 8 KiB cartridges and single-segment EA5 RAM programs.
.DEFAULT_GOAL := all

LLVM_BUILD = ../llvm-project/build/bin
CC = $(LLVM_BUILD)/clang
LD = $(LLVM_BUILD)/ld.lld
OBJCOPY = $(LLVM_BUILD)/llvm-objcopy
OBJDUMP = $(LLVM_BUILD)/llvm-objdump
READELF = $(LLVM_BUILD)/llvm-readelf
PYTHON = python3
BIN2RPK = ../bin2rpk.py
BIN2EA5 = ../bin2ea5.py

CPPFLAGS =
CFLAGS = --target=tms9900 -O3 -fno-builtin -ffreestanding -Wno-main-return-type
ASFLAGS = --target=tms9900
LDFLAGS = -T cart.ld --print-memory-usage
EA5_LDFLAGS = -T ea5.ld --print-memory-usage
STACK_SIZE = 0x800
STACK_FLAG = --defsym=__stack_size=$(STACK_SIZE)
EA5_BASE = 0xA000
EA5_MAX_SIZE = 0x6000

PROGRAMS = banner banner2 ball ball2 life life2 life2x life2_2x life3_2x kb_test irq_test
KEYBOARD_PROGRAMS = life2x life2_2x life3_2x kb_test
OBJECTS = crt0.o kbd.o life3_2x_step.o $(PROGRAMS:%=%.o)
OUTPUTS = $(OBJECTS) $(foreach suffix,elf ea5.elf bin ea5.bin cart rpk img list ea5.list,$(PROGRAMS:%=%.$(suffix)))

.PHONY: all clean
# Keep intermediates for inspection and later partial builds.
.SECONDARY:
all: $(PROGRAMS:%=%.bin) $(PROGRAMS:%=%.cart) $(PROGRAMS:%=%.rpk) $(PROGRAMS:%=%.img)

%.o: %.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -MMD -MP -MF $(@:.o=.d) -c $< -o $@

%.o: %.S
	$(CC) $(CPPFLAGS) $(ASFLAGS) -MMD -MP -MF $(@:.o=.d) -c $< -o $@

crt0.o: crt0.s
	$(CC) $(ASFLAGS) -c $< -o $@

# All demos share byte-exact .data/.bss initialization and an aligned C stack.
%.elf: crt0.o %.o cart.ld
	$(LD) $(STACK_FLAG) $(LDFLAGS) $(filter %.o,$^) -o $@
	$(OBJDUMP) -d $@ > $(@:.elf=.list)

%.ea5.elf: crt0.o %.o ea5.ld
	$(LD) $(STACK_FLAG) $(EA5_LDFLAGS) $(filter %.o,$^) -o $@
	$(OBJDUMP) -d $@ > $(@:.elf=.list)

$(KEYBOARD_PROGRAMS:%=%.elf) $(KEYBOARD_PROGRAMS:%=%.ea5.elf): kbd.o
life3_2x.elf life3_2x.ea5.elf: life3_2x_step.o

%.ea5.bin: %.ea5.elf
	$(OBJCOPY) -O binary --only-section=.text --only-section=.rodata --only-section=.data $< $@

%.bin: %.elf
	$(OBJCOPY) -O binary --only-section=.cart_header --only-section=.cart_entry --only-section=.text --only-section=.rodata --only-section=.data $< $@

%.cart: %.bin
	cp $< $@

# EA5 payloads are RAM executables, not cartridge ROMs. Do not wrap them as RPK.
$(PROGRAMS:%=%.rpk): %.rpk: %.bin $(BIN2RPK)
	$(PYTHON) $(BIN2RPK) $< -o $@

%.img: %.ea5.bin $(BIN2EA5)
	$(PYTHON) $(BIN2EA5) --max-size $(EA5_MAX_SIZE) -a $(EA5_BASE) $< -o $@

%.list: %.elf
	$(OBJDUMP) -d $< > $@

# More specific than %.list, so an EA5 listing cannot build an EA5 cartridge.
%.ea5.list: %.ea5.elf
	$(OBJDUMP) -d $< > $@

dump-%: %.elf
	$(READELF) -S -s $<
	$(OBJDUMP) -d $<

hexdump-%: %.bin
	xxd $< | head -20

clean:
	rm -f *.o *.d *.elf *.bin *.cart *.rpk *.list *.img .build-config.json

CONFIG_STAMP = .build-config.json
CONFIG_ARGS = --value=$(call quote_config,CPPFLAGS=$(CPPFLAGS)) \
              --value=$(call quote_config,CFLAGS=$(CFLAGS)) \
              --value=$(call quote_config,ASFLAGS=$(ASFLAGS)) \
              --value=$(call quote_config,LDFLAGS=$(LDFLAGS)) \
              --value=$(call quote_config,EA5_LDFLAGS=$(EA5_LDFLAGS)) \
              --value=$(call quote_config,STACK_SIZE=$(STACK_SIZE)) \
              --value=$(call quote_config,EA5_BASE=$(EA5_BASE)) \
              --value=$(call quote_config,EA5_MAX_SIZE=$(EA5_MAX_SIZE)) \
              --tool=$(call quote_config,$(CC)) --tool=$(call quote_config,$(LD)) \
              --tool=$(call quote_config,$(OBJCOPY)) --tool=$(call quote_config,$(OBJDUMP)) \
              --file=$(call quote_config,$(BIN2RPK)) --file=$(call quote_config,$(BIN2EA5))
CONFIG_OUTPUTS = $(OUTPUTS)
CONFIG_PATTERNS = *.o *.d *.elf *.bin *.cart *.rpk *.list *.img
include ../tools/build_config.mk
-include $(wildcard *.d)
