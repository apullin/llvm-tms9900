# TMS9900 Assembly & Optimization — Core Constitution

High-density field manual of **non-obvious hardware laws, hostile priors, exact cycle physics, and golden idioms** for the Texas Instruments TMS9900 (1976). Every rule below overrides x86/ARM/68000/6502/MSP430 muscle memory where those priors fail on TMS9900.

---

## 1. Non-Negotiable Hardware & Assembler Laws (Lethal Traps)

### 1.1 LLVM/Clang TMS9900 Assembly Dialects (`.S` GNU vs. `.asm`/`.a99` `xas99`)
`clang --target=tms9900` supports **two** dialects—never mix directives from one inside the other:
1. **GNU/Clang `.S` / `.s` (default for `.S` and `.s`)**:
   - **Comments**: **ALWAYS use `;` (or `/* ... */` / `//`)**. NEVER use `#` or column-1 `*` in `.S`!
   - **Hex & Directives**: Prefer `0x1234` (`>1234` also works); use `.text`, `.data`, `.bss`, `.p2align 1`, `.globl sym`, `.word`/`.short`, `.byte`, `.ascii`, `.space N`, and `label:` with colons.
2. **Modern `xas99` `.asm` / `.a99` (automatic on `.asm`/`.a99` or via `-masm=xas99`, disasm via `llvm-objdump -d -M xas99`)**:
   - Uses `DEF`, `REF`, `EQU`, `WEQU`, `REQU`, `DATA`, `BYTE`, `TEXT`, `STRI`, `FLOA`, `BSS`, `BES`, `EVEN`, `AORG`/`RORG`/`DORG`, `!` / `-!` local labels, `s#sym` byte size, `b#`/`w#` + `AUTO` pools, `*` or `;` comments, and `>1234` hex. Keep `xas99` syntax when editing `.asm`/`.a99` files!

### 1.2 Endianness, TI Bit Numbering, and the `MOVB` High-Byte Law
- **Big-endian memory**: Word at even address `A` has **MSB at `A`** and **LSB at `A + 1`**. Word accesses ignore bit `0` of the address (`A & 0xFFFE`).
- **TI manuals number bits MSB-first** (`bit 0` = MSB `0x8000`, `bit 15` = LSB `0x0001`). Standard masks: sign bit = `0x8000`, byte high lane = `0xFF00`, byte low lane = `0x00FF`.
- **ALL byte instructions (`MOVB`, `AB`, `SB`, `CB`, `SOCB`, `SZCB`) operate on the HIGH BYTE (`bits [15:8]`, mask `0xFF00`) when an operand is a register!**
  - `MOVB *R0, R1` loads 1 byte from `*R0` into **`R1` high byte** (`R1[15:8]`), leaving **`R1[7:0]` UNCHANGED (garbage!)**, and sets `LGT, AGT, EQ, OP` from that byte alone.
  - To zero-extend a loaded byte to `0..255` in `R1`: `MOVB *R0, R1` then `SRL R1, 8`.
  - To sign-extend a loaded `int8_t` to `-128..127` in `R1`: `MOVB *R0, R1` then `SRA R1, 8`.
  - To store the low byte of `R1` to `*R0`: `SWPB R1` (or `SLA R1, 8`), then `MOVB R1, *R0` — OR if `WP` is known (e.g. `WP = 0x8300`), `MOVB @0x8303, *R0` reads `R1`'s low byte directly from workspace RAM!
  - Memory-to-memory byte ops (`MOVB *R0+, *R1+`, `CB *R0+, *R1+`) transfer/compare single bytes directly with **no** shift needed and increment byte pointers by **`+1`** (word `*Rn+` increments by **`+2`**).

### 1.3 `C` (Compare) vs. `S` (Subtract) Operand Order & Jump Mnemonics — CRITICAL!

```text
╔══════════════════════════════════════════════════════════════════════════════╗
║  CRITICAL `C` OPERAND ORDER LAW — DO NOT REVERSE LIKE 68000 / AT&T `CMP`!  ║
║  In `C A, B` / `CB A, B` / `CI A, imm`, EVERY jump reads LEFT-TO-RIGHT:    ║
║    C A, B ; JEQ  ==> jumps if  A == B                                      ║
║    C A, B ; JNE  ==> jumps if  A != B                                      ║
║    C A, B ; JGT  ==> jumps if  A >  B  (signed)                            ║
║    C A, B ; JLT  ==> jumps if  A <  B  (signed)                            ║
║    C A, B ; JH   ==> jumps if  A >  B  (unsigned "High")                   ║
║    C A, B ; JL   ==> jumps if  A <  B  (unsigned "Low")                    ║
║    C A, B ; JHE  ==> jumps if  A >= B  (unsigned "High or Equal")          ║
║    C A, B ; JLE  ==> jumps if  A <= B  (unsigned — WARNING: NOT signed!)   ║
║  Example 1: to branch when `R0 >=u R2`, write `C R0, R2` then `JHE`        ║
║             (NEVER `C R2, R0 ; JHE` — that tests `R2 >=u R0`!).            ║
║  Example 2: `C @min, R5 ; JH` tests `@min >u R5`! To test `R5 >u @min`,    ║
║             ALWAYS put `R5` FIRST: `C R5, @min ; JH` (or `C @min, R5; JL`)!║
╚══════════════════════════════════════════════════════════════════════════════╝
```

- **There is NO signed `JGE` and NO signed `JLE` instruction!**
  - For signed `A >= B`: emit `C A, B` followed by `JLT false_label` (fallthrough is `A >= B`), OR `C A, B; JGT true_lbl; JEQ true_lbl`, OR swap operands: `C B, A; JGT false_lbl`!
  - For signed `A <= B`: emit `C A, B` followed by `JGT false_label` (fallthrough is `A <= B`), OR `C B, A; JLT false_lbl`!
- **`S src, dst`** computes **`dst = dst - src`** (SECOND minus FIRST — unlike `C`, which is FIRST vs SECOND!):
  - **SUBTRACT / `DEC` / `DECT` CARRY POLARITY**: Sets **`Carry = 1` (`JOC`) when NO borrow occurred (`dst >=u src`)**, and **`Carry = 0` (`JNC`) when a BORROW occurred (`dst <u src`)**!

### 1.4 Exact Status Register (`ST`) Behavior Matrix
Almost every instruction touches `ST`—except the ones that don't! Exploiting this eliminates `CI Rx, 0` and preserves `Carry` across logic/moves:

| Instructions | `LGT, AGT, EQ` | `C` (Carry) | `OV` (Overflow) | Notes |
|---|---|---|---|---|
| `MOV`, `MOVB` | **SET** (val vs 0) | **unchanged** | **unchanged** | `MOV Rx,Rx` is the 2-byte zero/sign test! `MOVB` also sets `OP`. |
| `LI`, `ANDI`, `ORI`, `SOC`, `SZC`, `XOR`, `INV` | **SET** (res vs 0) | **unchanged** | **unchanged** | **`ORI` / `ANDI` / `MOV` preserve Carry!** |
| `CLR`, `SETO`, `SWPB` | **unchanged** | **unchanged** | **unchanged** | Touch **NO** flags! Do NOT put `JNE`/`JEQ` after `CLR`/`SWPB` expecting new flags. |
| `B`, `BL`, `JMP`, `Jcc`, `LWPI`, `STWP`, `STST` | **unchanged** | **unchanged** | **unchanged** | Control transfers never alter `ST`. |
| `A`, `AB`, `AI`, `INC`, `INCT` | **SET** | **1 = carry out** | **SET** | Unsigned overflow sets `C=1` (`JOC`). |
| `S`, `SB`, `DEC`, `DECT` | **SET** | **1 = NO borrow** | **SET** | Borrow (`dst <u src`) clears `C=0` (`JNC`)! |
| `NEG`, `ABS` | **SET** (**input** on `ABS`!) | `NEG`: `in==0?0:1`; `ABS`: unchanged | `in == 0x8000` | **`ABS` sets `LGT/AGT/EQ` from the ORIGINAL operand before negation!** |
| `SLA`, `SRA`, `SRL`, `SRC` | **SET** (res vs 0) | **last bit shifted out** | `SLA` only (sign change) | `SRL Rx,1` puts old LSB in `C`; `SLA Rx,1` puts old MSB in `C`. |
| `MPY` | **unchanged** | **unchanged** | **unchanged** | `MPY` sets NO status bits! |
| `DIV` | **unchanged** | **unchanged** | **1 if `hi >= div`** | If `Rd >= divisor`, `DIV` aborts in 16c with `OV=1` (`JNO` jumps if OK). |

### 1.5 Architectural Quirks That Bite Every Beginner
1. **`R0` CANNOT be an index register**: `@offset(R0)` is encoded identically to symbolic `@offset` (`Ts=10, S=0`). Index registers MUST be **`R1`..`R15`**. (`*R0` and `*R0+` indirect/auto-inc DO work on `R0`!)
2. **Shift count `0` uses `R0[3:0]` (bits 12–15), and `0` means `16`!**:
   - `SLA Rx, 1..15` shifts by immediate `1..15`.
   - `SLA Rx, 0` reads the shift count from the **low 4 bits of `R0`**. **If `(R0 & 0xF) == 0`, the CPU shifts by 16**, NOT 0! Always guard variable shifts for `count == 0` before `SLA/SRL/SRA Rx, 0`.
3. **`MPY Rs, Rd` and `DIV Rs, Rd` work on ANY `Rd` (`R0..R15`), NOT just even registers!**
   - `MPY src, Rd`: unsigned `16 × 16 -> 32`: multiplies `Rd` by `src`, writes **high 16 bits to `Rd`** and **low 16 bits to `Rd+1`**. Note `src` is a general memory/register source (`MPY *R14+, R0`, `MPY @const, R2`), while `Rd` MUST be a workspace register.
   - `DIV src, Rd`: unsigned `32 ÷ 16 -> 16q, 16r`: divides 32-bit `Rd : Rd+1` (`Rd` = high, `Rd+1` = low) by 16-bit `src`. Writes **quotient to `Rd`** and **remainder to `Rd+1`**. **Precondition: `Rd <u src`** (for a 16-bit dividend in `Rd+1`, you MUST `CLR Rd` before `DIV`!).
4. **No `AND` register-register instruction**: Use **`SZC mask_zeros, dst`** (`dst &= ~mask_zeros`, "Set Zeros Corresponding" = BIC!) or immediate **`ANDI Rx, imm`**, or `INV src; SZC src, dst`.
5. **No hardware stack for `BL`**: `BL @sub` stores the return PC in **`R11`**. A leaf function NEVER touches `R10` (`SP`) and returns in 1 word via **`B *R11`**. A non-leaf function saves `R11` (either in a free scratch register like `R12` if calling known assembly helpers, or on `*R10` stack before calling C ABI functions).
6. **Jump range**: `JMP` and `Jcc` have a signed 8-bit **word** displacement (`-128..+127` words = `-256..+254` bytes from `PC+2`). For far targets use `B @label` (4 bytes).
7. **Format-3 Register-Only Destination Trap (`COC`, `CZC`, `XOR`, `MPY`, `DIV`)**:
   Unlike `MOV`/`A`/`S`/`C`/`SOC`/`SZC`, **`COC`, `CZC`, `XOR`, `MPY`, and `DIV` require their SECOND operand (`dst`) to be a workspace REGISTER `R0..R15` — NEVER `@mem` or `*Rn`!** (`XOR @mem, Rdst` is legal; `XOR Rsrc, @mem` is an assembler error!)
   - **Asymmetric `COC`/`CZC` warning**: `COC mask, Rword` sets `EQ=1` iff `(mask & Rword) == mask` (all `1`-bits of `mask` are set in `Rword`), and `CZC mask, Rword` sets `EQ=1` iff `(mask & Rword) == 0`. Do **NOT** write `COC Rmask, @mem` (illegal) OR `COC @mem, Rmask` (wrong subset direction!)—first load `MOV @mem, Rtmp`, then `COC Rmask, Rtmp; JEQ .Lset`!

---

## 2. Exact TMS9900 Cycle & Byte Cost Model (`C + W·M`)

TMS9900 is **strictly non-pipelined** (zero instruction overlap). Total cycles are the exact sum of executed instructions. Optimizing TMS9900 means minimizing instruction fetches, memory operands, and shift counts while keeping hot state in workspace registers `R0–R15`.

| Instruction Class | Bytes | Base Cycles (`W=0`) | Notes |
|---|---:|---:|---|
| `MOV`, `MOVB`, `A`, `AB`, `S`, `SB`, `SOC`, `SOCB`, `SZC`, `SZCB`, `XOR` (reg,reg) | 2 | **14** | Add Table A/B below for memory operands |
| `C`, `CB`, `COC`, `CZC` (reg,reg) | 2 | **14** | 3 mem accesses (no dest write) |
| `CLR`, `SETO`, `INC`, `INCT`, `DEC`, `DECT`, `INV`, `SWPB` (reg) | 2 | **10** | Fastest register mutations! Prefer `CLR` over `LI 0`, `INC/INCT/DEC/DECT` over `AI ±1/2`, `SWPB` over `SLA/SRL 8`! |
| `NEG` (reg), `ABS` (pos / neg) | 2 | **12** / **12–14** | |
| `LI Rx, imm` | 4 | **12** | Faster than `AI`/`ANDI`/`ORI`/`CI` (14c)! |
| `AI`, `ANDI`, `ORI`, `CI` | 4 | **14** | |
| `SLA`, `SRA`, `SRL`, `SRC` (imm `C = 1..15`) | 2 | **`12 + 2·C`** | `C=1`: 14c; `C=8`: **28c** (`SWPB` is **10c** — 18c faster!); `C=15`: **42c**! |
| `SLA`, `SRA`, `SRL`, `SRC` (`C = 0`, count in `R0`) | 2 | **`20 + 2·N`** | Costs +8c overhead over immediate shift |
| `JMP`, `Jcc` (not taken / taken) | 2 | **8 / 10** | Untaken jump is cheaper than taken! |
| `B *Rx` / `B @sym` | 2 / 4 | **12 / 16** | `B *R11` return is 12c |
| `BL *Rx` / `BL @sym` | 2 / 4 | **16 / 20** | Leaf call + return = 32c total |
| `BLWP @vec` / `RTWP` | 4 / 2 | **34 / 14** | Swaps entire 16-register workspace atomically! |
| `MPY Rs, Rd` | 2 (+addr) | **52** (+addr) | Constant time regardless of data! |
| `DIV Rs, Rd` (normal / overflow) | 2 (+addr) | **92–124 / 16** | Data-dependent |

**Addressing Mode Additions (per source or destination operand):**
- `Rx` (register direct): **`+0` B, `+0` cycles**
- `*Rx` (indirect): **`+0` B, `+4` cycles**
- `*Rx+` (auto-increment): **`+0` B, `+6` cycles (byte) / `+8` cycles (word)** — *cheaper AND 2–4 bytes smaller than `*Rx` + `INC/INCT Rx` (14–18c)!*
- `@sym` (symbolic) or `@off(Rx)` (indexed, `Rx != R0`): **`+2` B, `+8` cycles**

---

## 3. TMS9900 C Calling Convention (ABI)

| Register | Role | Preserved across call? |
|---|---|---|
| `R0–R3` | First 4 argument words (`i32` in `R0:R1`, `R2:R3` high-word first); return in `R0` (`16b`), `R0:R1` (`32b`), `R0:R1:R2:R3` (`64b`) | Caller-saved (clobber freely) |
| `R4–R9`, `R12` | Scratch registers (**7 free scratch regs** in every leaf function without touching stack!) | Caller-saved (clobber freely) |
| `R10` | Stack Pointer (`SP`, grows downward, **4-byte aligned**). 5th+ arg words enter at `@0(R10)`, `@2(R10)`, ... | **MUST preserve** |
| `R11` | Link Register (`LR`, return PC). Return via `B *R11`. | **MUST return to incoming `R11`** |
| `R13–R15` | Callee-saved registers (also `BLWP` saved `WP`, `PC`, `ST`) | **MUST preserve** (push/pop on `R10` only if you need >11 registers!) |

> **Golden Rule of Register Allocation**: A leaf function has **11 free registers (`R0–R9`, `R12`)** that require **zero prologue/epilogue saves and zero stack adjustments**! Almost never touch `R10` or `R13–R15` in hand-written assembly unless you have >11 live values or call external C functions.
>
> **Three Critical C↔ASM & Inline-ASM Seam Laws**:
> 1. **Split `R3:*R10` Multi-Word Args**: Clang splits `i32`/`i64` args into 16-bit halves (`hi` then `lo`) *before* assigning `R0..R3`; remaining 16-bit halves spill to `*R10`, `@2(R10)`, `@4(R10)` (2-byte aligned—an `i32` starting at word #3 puts **`hi` in `R3` and `lo` at `*R10`**!). Variadic (`...`) calls pass **all** args on the stack starting at `*R10`.
> 2. **C `uint8_t`/`int8_t` Low-Byte Promotion vs. `MOVB` High-Byte**: C promotes `uint8_t`/`int8_t` into the **LOW byte (`0x00XX` / `0xSSXX`)** of a 16-bit register—opposite of `MOVB` (`0xXX00`)! Always `SWPB` before `MOVB` when consuming a C `uint8_t` register (including in C `__asm__ volatile`!), and `SRL R0, 8` / `SRA R0, 8` when returning `uint8_t`/`int8_t` to C.
> 3. **C Inline-ASM Constraints**: Use earlyclobber `"=&r"` on scratch/output registers written before inputs are consumed; for `MPY` in inline asm, explicitly allocate an adjacent register pair (e.g., clobber `"r2", "r3"` and move into `R2` -> `MPY src, R2`) because `MPY` overwrites `Rd` AND `Rd+1`!

---

## 4. The Golden TMS9900 Assembly Idioms

### Idiom 1: Zero/Sign Tests Without `CI`
```asm
MOV  R0, R0        ; 2B, 14c (vs CI R0,0 = 4B, 14c): sets EQ (==0), JLT (<0), JGT (>0)
; Even better: if preceding A/S/MOV/ANDI/ORI/DEC/SLA already wrote R0, omit MOV R0,R0 completely!
```

### Idiom 2: Multi-Word (`32`/`64`-bit) Add & Subtract via Hardware Carry
- **32-bit Add (`R0:R1 += R2:R3`)** — 4 insns, 8B:
  ```asm
  A    R2, R0        ; add high words
  A    R3, R1        ; add low words, sets C=1 on unsigned carry
  JNC  .Lno_c
  INC  R0
  .Lno_c:
  ```
- **32-bit Subtract (`R0:R1 -= R2:R3`)** — 4 insns, 8B (**remember `JOC` = NO borrow!**):
  ```asm
  S    R2, R0        ; sub high words
  S    R3, R1        ; sub low words; C=1 if NO borrow, C=0 if borrow!
  JOC  .Lno_b
  DEC  R0
  .Lno_b:
  ```
- **Cascaded 64-bit `*dst -= *src` (`R0 = dst`, `R1 = src`, words `@0..@6` high-to-low)**:
  Watch out for **`dst == src` aliasing** AND **borrow-cascade through `0x0000` words** (`DEC` on `0x0000 -> 0xFFFF` sets `C = 0`, whereas `DEC` on `>= 1` sets `C = 1`!):
  ```asm
  ; Clean, alias-safe 4-word big-endian *R0 -= *R1 (move dst to R2!=R0 so @off(R2) is legal!):
      MOV  R0, R2           ; R2 != R0 so indexed @-k(R2) is valid!
      S    *R1+, *R2+       ; word 0 (MSB): R2 is now &dst[1]
      S    *R1+, *R2+       ; word 1: R2 is now &dst[2]
      JOC  .Lw2
      DEC  @-4(R2)          ; borrow into word 0
  .Lw2:
      S    *R1+, *R2+       ; word 2: R2 is now &dst[3]
      JOC  .Lw3
      DEC  @-4(R2)          ; borrow into word 1 (sets C=0 iff old word 1 was 0!)
      JOC  .Lw3
      DEC  @-6(R2)          ; ripple borrow into word 0!
  .Lw3:
      S    *R1, *R2         ; word 3 (LSB): R2 stays &dst[3]
      JOC  .Ldone
      DEC  @-2(R2)          ; borrow into word 2 (sets C=0 iff old word 2 was 0!)
      JOC  .Ldone
      DEC  @-4(R2)          ; ripple into word 1
      JOC  .Ldone
      DEC  @-6(R2)          ; ripple into word 0
  .Ldone:
  ```

### Idiom 3: Arbitrary `uint32 / uint16` Division in AT MOST TWO `DIV`s (~220c vs ~7,500c!)
Hardware `DIV divisor, Rd` overflows if `Rd >=u divisor`. When `num` (`R0:R1`) has `R0 >=u den` (`R2`), split into two non-overflowing hardware `DIV`s:
```asm
; R0:R1 = 32-bit num, R2 = 16-bit den (>0), R3 = rem_out ptr
    C    R0, R2        ; R0 <u R2 ?
    JHE  .Ltwo_div     ; if R0 >=u R2, single DIV would overflow!
    DIV  R2, R0        ; R0 = quot_lo (quot_hi = 0), R1 = rem
    MOV  R1, *R3       ; *rem_out = rem
    MOV  R0, R1
    CLR  R0            ; R0:R1 = 0 : quot_lo
    B    *R11
.Ltwo_div:
    CLR  R4            ; R4:R5 = 0 : num_hi (R4=0 < R2, CANNOT overflow!)
    MOV  R0, R5
    DIV  R2, R4        ; R4 = quot_hi, R5 = rem_hi (< R2!)
    MOV  R5, R0        ; R0:R1 = rem_hi : num_lo (R0 < R2, CANNOT overflow!)
    DIV  R2, R0        ; R0 = quot_lo, R1 = final_rem
    MOV  R1, *R3       ; *rem_out = final_rem
    MOV  R0, R1        ; low word of quotient
    MOV  R4, R0        ; high word of quotient
    B    *R11
```

### Idiom 4: Signed `16 × 16 -> 32` Multiply Using Unsigned `MPY` (Hacker's Delight)
`MPY` is unsigned, but signed `a * b` in `R0:R1` needs **no `ABS`/`NEG` branches on the 32-bit product**—just subtract `b` from the high word if `a < 0`, and subtract `a` from the high word if `b < 0`:
```asm
; Input: R0 = signed a, R1 = signed b -> Output: R0:R1 = signed 32-bit a*b
    MOV  R0, R2        ; save a
    MOV  R1, R3        ; save b
    MPY  R3, R0        ; R0:R1 = (uint16)a * (uint16)b
    MOV  R2, R2
    JGT  .Lchk_b
    JEQ  .Lchk_b
    S    R3, R0        ; if a < 0: high_word -= b
.Lchk_b:
    MOV  R3, R3
    JGT  .Ldone
    JEQ  .Ldone
    S    R2, R0        ; if b < 0: high_word -= a
.Ldone:
```
And to extract **`(p >> 8)`** from signed 32-bit `R0:R1` into `R2`:
`MOV R0, R2; SWPB R2; ANDI R2, 0xFF00; MOV R1, R4; SRL R4, 8; SOC R4, R2` — OR `SRL R1, 8; SLA R0, 8; SOC R0, R1` (after checking saturation on `R0:R1`!).

### Idiom 5: Dual Auto-Increment Byte/Word Streams & `SWPB` Byte Tricks
- **Block copy**: `MOV *R1+, *R2+` (2B, 30c per 2 bytes) or `MOVB *R0+, *R2+` (2B, 26c per byte — **never** write `MOVB *R0+, *R2; INC R2`!).
- **Byte compare scan**: `CB *R0+, *R1+; JNE .Ldiff` (compares `*s1++` and `*s2++` in 1 word!).
- **CRC-16 MSB-first gift**: `MOVB *R0+, R2` ALREADY loads `*buf++` into the **high byte** (`b << 8`)! Just `ANDI R2, 0xFF00; XOR R2, R3` (or `XORB` does not exist—clear `R2` once before the outer loop with `CLR R2`, then `MOVB *R0+, R2` leaves `R2`'s low byte `0x00`, so **`XOR R2, R3` needs NO `ANDI` AND NO SHIFT AT ALL!**).
- **1-bit Cross-Word Shift via Carry + `ORI` (`ORI` preserves Carry!)**:
  `SRL R0, 1; SRL R1, 1`? Careful: shift the source of the transferred bit second, or `SLA R0, 1; SLA R1, 1; JNC $+4; INC R0`.
- **Fast SWAR `popcount16` / `bitrev16` / `clz16`**:
  - On TMS9900, multi-bit shifts cost `12 + 2·C`, whereas **`SWPB` is 10c** (swaps bytes in 1 cycle class!).
  - For `bitrev16`: swap bits `(1,2,4)` with `0x5555`, `0x3333`, `0x0F0F` (`MOV` + `SRL` + `ANDI` + `ANDI` + `SLA` + `SOC`), then finish the 8-bit swap in **10 cycles with `SWPB R0`**!
  - For `popcount16`: SWAR 2-bit/4-bit/8-bit reduction, then `MPY` or `MOV R0,R1; SWPB R1; A R1,R0; SRL R0,8; ANDI R0,0x001F` (uses 10c `SWPB` instead of `SRL 8`!).
  - For `clz16`: binary search (`CI R0, 0x0100; JHE` or `MOV R0,R0; JEQ` -> `SLA` 8/4/2/1; note `SLA R0, 8` or checking `< 0x0100` with `CI R0, 0x0100; JHE` is unsigned!).

### Idiom 6: `BLWP` Vectors, Caller Workspace (`R13`), and Inline `DATA` (`*R14+`)
When invoked via `BLWP @vec` (`vec: .short my_ws, my_entry`), the CPU loads `WP = my_ws`, `PC = my_entry`, and places in `my_ws`:
- **`R13`** = caller's `WP` (`*R13` is caller's `R0`, `@2(R13)` is caller's `R1`, `@2*k(R13)` is caller's `Rk`!)
- **`R14`** = caller's return `PC` (`MOV *R14+, R0` fetches an inline `DATA` word right after `BLWP @vec` AND advances caller's return `PC` by `+2`!)
- **`R15`** = caller's `ST`
- **`RTWP`** restores `WP = R13, PC = R14, ST = R15` in 14 cycles while leaving `my_ws`'s `R0–R12` intact in RAM for the next `BLWP` call (instant zero-overhead coroutines/generators!).

### Idiom 7: TI-99 Data-Structure Speedups (Pre-Scaled Hash Masks & Zero-Wait PAD Scratch)
- **Power-of-2 `{key, val}` Hash Tables (`4` bytes/slot, empty sentinel `0xFFFF`)**:
  Mix high & low key bits without `DIV` (`MOV R1, R3; SWPB R3; XOR R1, R3`, plus optional `MOV R3, R4; SRL R4, 3; XOR R4, R3`), then mask **directly into a 4-byte word-pair byte offset** `ANDI R3, ((cap - 1) << 2)` (e.g. `0x00FC` for 64 slots) — zero `SLA 2` shifts needed! Advance linear probes by **4 bytes** with **`AI R3, 4; ANDI R3, 0x00FC`** (**NEVER `INCT`**, which only adds `+2` and gets zeroed by `ANDI 0x00FC`!).
- **Exploiting 16-bit Zero-Wait Scratchpad PAD (`0x8320–0x836F` = 80 bytes when `LIMI 0`)**:
  On real TI-99/4A hardware, 32K Expansion RAM (`0x2000`/`0xA000`) incurs **+4 wait clocks per 16-bit word access**, whereas `0x8300–0x83FF` Scratchpad PAD has **0 wait states**! Placing small auxiliary data structures (8–16 word min-heaps, monotonic index stacks, 128-bit ASCII bitsets, or inner blitter loops) in `0x8320–0x836F` cuts memory cycles by **~40%**!
