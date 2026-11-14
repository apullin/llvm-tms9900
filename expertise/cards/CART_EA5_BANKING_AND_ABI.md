# TI-99/4A Cartridge Headers, ROM Banking, EA5, & `BLWP`/C ABI Card

---

## 1. Cartridge ROM Header at `0x6000` (`>6000–>7FFF`)

The TI-99/4A Console scans `0x6000` at boot for byte `0xAA` and builds the title-screen selection menu from its Program List:

```asm
    .section .cart_header, "a"
    .byte  0xAA, 0x01       ; 0x6000: Standard header magic (0xAA) + version (0x01)
    .short 0x0000           ; 0x6002: Number of programs (ignored by console)
    .short 0x0000           ; 0x6004: Power-up list pointer (0 = none)
    .short prog_list        ; 0x6006: Program list pointer
    .short 0x0000           ; 0x6008: DSR list pointer (0 = none)
    .short 0x0000           ; 0x600A: Subprogram list pointer (0 = none)

prog_list:
    .short 0x0000           ; Next menu item pointer (0 = end of list)
    .short _start           ; Entry PC (must be even!)
    .byte  11               ; Pascal-style length byte of menu title!
    .ascii "MY TI99 APP"    ; Menu title (uppercase ASCII)
    .p2align 1              ; MUST realign to even boundary before instructions!

    .section .text
    .globl _start
_start:
    LIMI 0                  ; Disable console VDP/GPL interrupts immediately
    LWPI 0x8300             ; 16-bit 0-wait Scratchpad workspace (0x8300..0x831F)
    LI   R10, 0xFFFC        ; 4-byte-aligned C stack top in 32K expansion RAM
    ; Copy .data from ROM to RAM and zero .bss, then BL @main
```
- **Console-only 16K VDP + 256B PAD Cartridges (No 32K Expansion RAM!)**: If targeting an unexpanded console, **only `0x8300–0x83FF` (256 bytes) of CPU RAM exists**—store workspace at `0x8300`, variables/tiny stack in `0x8320–0x836E`, and bulk game state in 16KB VRAM (`0x0000–0x3FFF`)!

---

## 2. Write-to-ROM Cartridge Bank Switching (`378` / `379` Non-Inverted Latch)

Cartridges > 8 KB (`16K..512K`, MAME/FlashROM99/FinalGROM99 `paged378`) switch the entire `0x6000–0x7FFF` window by performing a dummy write to `0x6000 + 2 * bank`:
- `CLR @0x6000` -> selects Bank `0`
- `CLR @0x6002` -> selects Bank `1`
- `CLR @0x6000(Rx)` (with `Rx = 2 * bank`, `Rx != R0`!) -> selects Bank `bank`!
- **CRITICAL BANK-SWITCHING RULES**:
  1. **Every bank MUST carry a valid `0xAA` header at `0x6000`** (power-on latch state is indeterminate on real 74LS378 hardware!), with `_start` in every bank switching to Bank `0` (`CLR @0x6000`).
  2. Never execute a bank switch directly from an address in `0x6000–0x7FFF` unless that exact PC address contains the continuation code in the **destination** bank—or put a tiny 12-byte bank-call trampoline in **Scratchpad PAD (`0x8320`)**!

---

## 3. Editor/Assembler Option 5 (`EA5`) RAM Executable Format

EA5 files load straight into 32K Expansion RAM (typically `0xA000` or `0x2000`) via a **6-byte big-endian header** prepended to each up-to-8KB image chunk:
- Word `0` (`0x00`): `0x0000` = **last file**, `0xFFFF` = **more segment files follow** (increment last char of filename).
- Word `1` (`0x02`): **Payload byte count** (excludes the 6-byte header itself!).
- Word `2` (`0x04`): **Load address** (e.g. `0xA000`; execution begins at the first segment's load address!).

---

## 4. `BLWP` / `RTWP` / `XOP` Context-Switch & Coroutine Patterns

Unlike stack-based CPUs, `BLWP @vector` (`vector: .short new_wp, new_pc`) swaps **all 16 registers in 34 cycles** by changing `WP`, storing the caller's context into the **callee's** workspace:
- Callee `R13` = caller `WP`
- Callee `R14` = caller return `PC`
- Callee `R15` = caller `ST`

### Accessing Caller Registers & Inline `DATA` Arguments from a `BLWP` Handler
```asm
; Read caller's Rk (0..15): @2*k(R13)  (*R13 is caller's R0!)
    MOV  *R13, R0           ; load caller's R0
    MOV  @2(R13), R1        ; load caller's R1
    MOV  R2, *R13           ; write return value into caller's R0!

; Fetch inline DATA words placed right after caller's `BLWP @vec`:
    MOV  *R14+, R0          ; fetch 1st inline word AND advance return PC by +2!
    MOV  *R14+, R1          ; fetch 2nd inline word AND advance return PC by +2!
    RTWP                    ; resumes caller AFTER the inline DATA words!
```

### Zero-Overhead Stateful Coroutines / Generators via `BLWP`
Because `RTWP` does **not** touch `R0–R12` of the callee's workspace, any values left in `R0–R12` of `my_ws` **persist across `BLWP` calls** with zero memory loads/stores!

---

## 5. LLVM C ↔ Assembly Stack & Multi-Word ABI Details

When C calls assembly `f(a, b, c, ...)` (`BL @f`):
- **Register args (`4` word budget: `R0, R1, R2, R3`)**:
  - `8`-bit / `16`-bit scalar or pointer -> 1 register (`uint8_t` zero-extended, `int8_t` sign-extended in low byte!).
  - `32`-bit scalar (`int32_t` / `uint32_t` / `float`) -> **2 words**, high word first (`R0:R1`, `R2:R3`). If 3 registers are already used (`R0, R1, R2`), a 32-bit 4th argument splits **`high -> R3`**, **`low -> *R10`**!
  - `64`-bit scalar -> **4 words** (`R0:R1:R2:R3`).
- **Stack args (5th word onward)**:
  - Placed by caller starting at **`*R10` (`@0(R10)`)**, then `@2(R10)`, `@4(R10)`, ... (**before** callee modifies `R10`! Note `BL` does NOT push `R11`, so at function entry the 5th argument word is right at **`*R10` (`@0(R10)`)**!).
  - For a 32-bit argument on the stack at `@k(R10)`: **high word is at `@k(R10)`**, **low word is at `@(k+2)(R10)`**.
- **Stack alignment**: If your assembly routine calls a C function, `R10` **MUST be 4-byte aligned (`R10 % 4 == 0`)** at the `BL` site (e.g. `AI R10, -4; MOV R11, *R10; BL @c_fn; MOV *R10, R11; AI R10, 4`).
