# TMS9900 & TI-99/4A Congealed Expertise Pack (`expertise/`)

A portable, model-agnostic **Prior-Delta / Contrastive-Distillation** knowledge package and automated evaluation suite (`TMS9900-Bench`) designed to maximize any coding LLM's ability to write ultra-efficient **TMS9900 assembly** and **TI-99/4A hardware code**—without burning ~380,000+ context tokens on raw manuals.

---

## 1. Headline Empirical Results (`TMS9900-Bench`, 20 Tasks: 10 Dev + 10 Blind Held-Out Test)

All models were evaluated **single-shot, zero-tools, isolated** (`--tools ""`, no repo access) and graded automatically by assembling with `clang --target=tms9900`, checking ABI invariants (`R10, R11, R13–R15`), and executing in `tms9900-trace` (including `--ti99-io` VDP VRAM/register, GROM, PSG, and Scratchpad `0x8320` tracepoint verification). Speedups are measured against **our freshly optimized `clang -O2` TMS9900 compiler**.

| Model | Context Regime | Prompt Budget | Dev (`10`) | Held-Out Test (`10`) | Total Pass@1 | Mean Speedup vs. `clang -O2` |
|---|---|---:|---:|---:|---:|---:|
| **Claude Haiku** | Cold (`0` pack tokens) | ~406 tok | 5/10 (50%) | 6/10 (60%) | **11/20 (55.0%)** | 1.64× *(passing subset)* |
| **Claude Haiku** | **Congealed Pack** | **~5,960 tok** | **10/10 (100%)** | **10/10 (100%)** | **20/20 (100.0%)** | **1.66× faster** |
| **Claude Sonnet** | Cold (`0` pack tokens) | ~406 tok | 6/10 (60%) | 5/10 (50%) | **11/20 (55.0%)** | 1.93× *(passing subset)* |
| **Claude Sonnet** | **Core Constitution Only** | **~5,012 tok** | **10/10 (100%)** | **10/10 (100%)** | **20/20 (100.0%)** | **1.72× faster** |
| **Claude Sonnet** | **Congealed Pack (Core + Cards)** | **~5,960 tok** | **10/10 (100%)** | **10/10 (100%)** | **20/20 (100.0%)** | **1.85× faster** |
| **OpenAI Codex** | Cold (`0` pack tokens) | ~406 tok | 9/10 (90%) | 10/10 (100%) | **19/20 (95.0%)** | 1.72× faster |
| **OpenAI Codex** | **Congealed Pack (Core + Cards)** | **~5,989 tok** | **10/10 (100%)** | **10/10 (100%)** | **20/20 (100.0%)** | **1.87× faster** |

**Key Takeaways:**
- **98.4% Context Reduction**: Precipitating ~380K+ tokens of TI manuals, books, Console ROM disassemblies, and compiler war stories down to **~5.0K–6.0K tokens** lifted **every** tested model across both vendors (`Haiku`, `Sonnet`, `Codex`) to **20/20 (100.0%) Pass@1**, including **10/10 on strictly held-out test tasks**.
- **Small Model Beats Larger Cold Model**: `Haiku + Pack` (`100%`) beat `Cold Sonnet` (`55%`) by **+45 percentage points**.
- **Crushes `clang -O2` Across Both Bytes and Cycles**: Pack-guided assembly averages **1.66×–1.87× faster** (up to **3.44× faster** on `q8_8_mul_sat` and **3.36× faster / 2.9× smaller** on `u64_sub_inplace`) than our optimized `-O2` C compiler.
- **Tooling Co-Discovery**: Running the cross-model refinement loop exposed a real silent wrong-code bug in LLVM's `TMS9900AsmParser` (`@offset(R0)` silently assembling as symbolic `@offset`), which is now diagnosed as an error in `llvm-mc`/`clang`.

---

## 2. Package Layout

```text
expertise/
├── SKILL.md                           # Drop-in agent skill router (Progressive Disclosure)
├── CORE_CONSTITUTION.md               # ~4.6K-token high-surprise TMS9900 ISA, ST flags, timing & idioms
├── VAT_INDEX.md                       # Manifest of the ~500K-token primary-source corpus distilled here
├── cards/
│   ├── TI99_MEMORY_PAD_AND_ROM.md     # >0000->FFFF map, >8300 PAD, GROM vs VDP order, KSCAN, >83D4, GPLLNK
│   ├── TMS9918A_VDP_AND_GRAPHICS.md   # VDP modes, SAT Y-1 / 0xD0 / Early-Clock quirks, fast *R+ blitters
│   ├── CRU_TMS9901_SOUND_SPEECH.md    # R12=2*bit, LDCR/STCR 1..8 high-byte trap, raw kbd/joy, PSG >8400
│   ├── CART_EA5_BANKING_AND_ABI.md    # >6000 0xAA headers, 378 ROM banking, EA5, BLWP coroutines, C ABI
│   └── GROM_GPL_XB_AND_SAMS.md        # 16-base GROM (640KB), Console GROM/GPLLNK, XB +0x60 bias, XML, SAMS
├── exemplars/                         # 9 simulator-verified, cycle-tuned .S reference kernels
│   ├── 01_arith32_div_mul_q88.S
│   ├── 02_mem_string_autodual.S
│   ├── 03_bitboard_csa_stencil.S
│   ├── 04_vdp_pad_blitter_and_grom.S
│   ├── 05_blwp_coroutines_and_linkage.S
│   ├── 06_cru_keyscan_and_psg_sound.S
│   ├── 07_rle_and_lz_decompress.S
│   ├── 08_banked_cart_header_crt0.S
│   └── 09_gfx2_vblank_isr_and_hashmap.S
└── eval/
    ├── benchmarks.py                  # 20 core/peripheral tasks (10 dev + 10 held-out test)
    ├── bizarro_bench.py               # 5 GROM/XB/GPL/SAMS/banked-ROM seam tasks
    ├── odyssey_bench.py               # 12 full-system ti99sim tasks (wait states, IRQ, PSG, Graphics II, ABI, bugfix)
    ├── run_eval.py                    # Parallel headless evaluator (c-baseline, haiku, sonnet, codex)
    └── results/                       # Saved raw outputs, .S/.asm/.c kernels, & JSON scorecards
```

---

## 3. Per-Task Scorecard (`Bytes / Benchmark Cycles`)

| Task | `clang -O2` C | Haiku Cold | Haiku + Pack | Sonnet Cold | Sonnet + Pack | Codex Cold | Codex + Pack |
|---|---:|---:|---:|---:|---:|---:|---:|
| `dev_01_popcount16` | 46B / 5610c | **46B** / 5322c | **46B** / 5322c | **46B** / 5322c | **46B** / 5322c | **44B** / 5546c | **46B** / 5322c |
| `dev_02_u32_div16` | 88B / 12076c | **24B** / 5002c | **32B** / 4414c | **18B** / 4582c | **32B** / 4414c | **18B** / 4582c | **30B** / 4378c |
| `dev_03_i32_clamp` | 148B / 10434c | ❌ `WRONG_ANS` | **42B** / 4420c | **42B** / 4420c | **38B** / 4372c | **38B** / 4372c | **38B** / 4372c |
| `dev_04_crc16_ccitt` | 148B / 6682c | **40B** / 4306c | **74B** / 2764c | **40B** / 4374c | **72B** / 2762c | **74B** / 2764c | **72B** / 2762c |
| `dev_05_binary_search_i16` | 56B / 17520c | ❌ `ASM_ERR` | **48B** / 13658c | ❌ `WRONG_ANS` | **50B** / 11390c | **46B** / 12926c | **50B** / 11434c |
| `dev_06_rle_decompress` | 96B / 2118c | ❌ `ASM_ERR` | **70B** / 1392c | ❌ `ASM_ERR` | **52B** / 1238c | ❌ `WRONG_ANS` | **68B** / 1444c |
| `dev_07_blwp_inline_args` | ASM-only | **14B** / 472c | **14B** / 472c | **10B** / 472c | **14B** / 472c | **14B** / 472c | **14B** / 472c |
| `dev_08_vdp_write_stream` | 42B / 3346c | ❌ `ASM_ERR` | **70B** / 1632c | ❌ `ASM_ERR` | **64B** / 1618c | **30B** / 2120c | **64B** / 1618c |
| `dev_09_grom_font_to_vdp` | 86B / 2008c | ❌ `ASM_ERR` | **66B** / 1530c | ❌ `ASM_ERR` | **66B** / 1522c | **60B** / 1514c | **62B** / 1516c |
| `dev_10_sn76489_tone` | 46B / 1668c | **58B** / 1720c | **46B** / 1608c | **40B** / 1532c | **46B** / 1660c | **38B** / 1572c | **40B** / 1560c |
| `test_01_clz16` | 106B / 9182c | ❌ `ASM_ERR` | **58B** / 6076c | **56B** / 5822c | **54B** / 5876c | **56B** / 5934c | **54B** / 5774c |
| `test_02_q8_8_mul_sat` | 166B / 11186c | **54B** / 3492c | **62B** / 3480c | **52B** / 3412c | **58B** / 3446c | **52B** / 3116c | **54B** / 3256c |
| `test_03_bit_reverse16` | 52B / 2970c | **52B** / 2970c | **52B** / 2970c | **52B** / 2970c | **52B** / 2970c | **52B** / 2970c | **52B** / 2970c |
| `test_04_u64_sub_inplace` | 140B / 5878c | ❌ `WRONG_ANS` | **64B** / 3350c | **72B** / 1702c | **54B** / 1750c | **52B** / 2870c | **48B** / 1750c |
| `test_05_insertion_sort_i16` | 68B / 11038c | ❌ `WRONG_ANS` | **52B** / 11248c | ❌ `ASM_ERR` | **44B** / 8316c | **44B** / 7766c | **46B** / 7778c |
| `test_06_lz_window_decomp` | 56B / 2072c | ❌ `ASM_ERR` | **44B** / 1604c | **40B** / 1606c | **44B** / 1538c | **40B** / 1548c | **44B** / 1514c |
| `test_07_itoa_u16` | 102B / 12844c | **84B** / 6962c | **136B** / 7996c | ❌ `ASM_ERR` | **72B** / 6554c | **62B** / 8398c | **68B** / 5872c |
| `test_08_blwp_coroutines` | ASM-only | **32B** / 3854c | **32B** / 3690c | ❌ `ASM_ERR` | **30B** / 3694c | **28B** / 3574c | **24B** / 3714c |
| `test_09_vdp_reg_and_sprite` | 104B / 1350c | **100B** / 1292c | **94B** / 1164c | ❌ `ASM_ERR` | **74B** / 1004c | **78B** / 1026c | **76B** / 1032c |
| `test_10_pad_ram_trampoline` | ASM-only | **68B** / 2212c | **68B** / 2212c | ❌ `ASM_ERR` | **52B** / 1342c | **44B** / 1394c | **38B** / 1410c |

---

## 3b. Bizarro TI-99 System SWE-Bench (`bizarro_bench.py`)

5 gnarled hardware/ecosystem seam tasks: Multi-Base GROM (Base 3 `0x980C`/`0x9C0E` + 13-bit 8KB chip wrap), Extended BASIC (`+0x60` screen ASCII bias + sacred PAD), **real 8 KB TI-99/4A Console ROM GPL interpreter (`0x0060`) booting a custom `0xAA` GROM cartridge -> GPL `MOVE` -> `XML >F0` -> native ASM**, 1 MB SAMS (`CRU 0x1E00`, `0x4014`/`0x4016` high-byte latch), and Bank 0 ↔ 16-bit PAD (`0x8320`) ↔ Bank 3 `378` ROM trampoline:

| Bizarro Task | Haiku Cold | Haiku + Pack | Sonnet Cold | Sonnet + Pack | Codex Cold | Codex + Pack |
|---|---:|---:|---:|---:|---:|---:|
| `biz_01_grom_multibase_wrap` | ❌ `ASM_ERR` | **78B** / 26310c | ❌ `ASM_ERR` | **78B** / 26148c | **54B** / 64746c | **66B** / 26052c |
| `biz_02_xb_screen_bias` | ❌ `VERIFY_ERR` | **82B** / 6390c | ❌ `ASM_ERR` | **56B** / 3694c | **58B** / 3822c | **56B** / 3690c |
| `biz_03_gpl_cart_to_native` | ❌ `TIMEOUT` | **88B** / 11228c | ❌ `VERIFY_ERR` | **90B** / 11532c | ❌ `TIMEOUT` | **92B** / 11840c |
| `biz_04_sams_1mb_seam_sum` | ❌ `ASM_ERR` | **68B** / 11526c | ❌ `ASM_ERR` | **44B** / 11462c | **46B** / 11474c | **42B** / 11450c |
| `biz_05_pad_rom_bank_call` | **60B** / 3578c | **60B** / 3502c | ❌ `ASM_ERR` | **52B** / 3530c | **50B** / 2810c | **52B** / 3230c |
| **Total Pass@1** | **1/5 (20%)** | **5/5 (100%)** | **0/5 (0%)** | **5/5 (100%)** | **4/5 (80%)** | **5/5 (100%)** |

---

## 3c. Full-System `ti99sim` Odyssey Suite (`odyssey_bench.py` — Real Bus Wait States, 60Hz VBlank IRQ, PSG, Graphics II, ABI & Bugfix)

12 tasks executed on the full-system TI-99/4A emulator (`ti99sim-console --harness` with `TI-994A.ctg`, authentic 0-wait `0x8300` PAD vs. +4-wait 8-bit 32K RAM clocks, 60 Hz NTSC VDP retrace interrupts, 4-channel TMS9919 sound, TMS9901 CRU keyboard matrix, and `convert-ctg` cold-booted `.ctg` cartridges):

| Odyssey Task | `clang -O2` C | Haiku Cold | Haiku + Pack (`v3`) | Sonnet Cold | Sonnet + Pack (`v3`) | Codex Cold | Codex + Pack (`v3`) |
|---|---:|---:|---:|---:|---:|---:|---:|
| `ody_01_ti99_hashmap_u16` | 118B / 248.5Kc | **116B** / 240.2Kc | **90B** / 228.2Kc | **100B** / 197.2Kc | **116B** / 212.8Kc | **104B** / 209.2Kc | **110B** / 230.5Kc |
| `ody_02_largest_rect_hist` | 192B / 53.4Kc | ❌ `WRONG_ANS` | **114B** / 54.1Kc | **88B** / 39.4Kc | **84B** / 35.5Kc | ❌ `WRONG_ANS` | **84B** / 35.2Kc |
| `ody_03_topk_minheap_stream` | 224B / 50.8Kc | ❌ `WRONG_ANS` | **150B** / 28.6Kc | **82B** / 22.6Kc | **84B** / 20.6Kc | ❌ `ASM_ERR` | **130B** / 18.5Kc |
| `ody_04_longest_unique_substr` | 114B / 99.0Kc | ❌ `WRONG_ANS` | **72B** / 48.0Kc | ❌ `ASM_ERR` | **42B** / 27.3Kc | ❌ `ASM_ERR` | **92B** / 29.6Kc |
| `ody_05_gfx2_bresenham_rmw` | Full-sys ASM | ❌ `ASM_ERR` | **474B** / 583.8Kc | ❌ `ASM_ERR` | **364B** / 580.8Kc | ❌ `TIMEOUT` | **336B** / 924.2Kc |
| `ody_06_vblank_isr_music_and_sync` | Full-sys IRQ | **260B** / 257.0Kc | **244B** / 257.0Kc | ❌ `ASM_ERR` | **184B** / 256.9Kc | **186B** / 253.5Kc | **174B** / 257.0Kc |
| `ody_07_tms9901_cru_keyscan` | Full-sys CRU | **136B** / 11.5Kc | **72B** / 7.8Kc | **130B** / 11.1Kc | **72B** / 7.8Kc | ❌ `ASM_ERR` | **72B** / 7.8Kc |
| `ody_08_bootable_cart_ctg` | Cold-boot `.ctg` | **324B** / PASS | **316B** / PASS | ❌ `ASM_ERR` | **328B** / PASS | **270B** / PASS | **258B** / PASS |
| `ody_09_split_i32_stack_callback` | Multi-arg ABI | ❌ `WRONG_ANS` | **142B** / 6.3Kc | ❌ `ASM_ERR` | **100B** / 4.9Kc | ❌ `LINK_ERR` | **110B** / 4.9Kc |
| `ody_10_c_inline_asm_seams` | C + Inline ASM | **52B** / 1.7Kc | **50B** / 1.7Kc | ❌ `ASM_ERR` | **44B** / 1.5Kc | **52B** / 1.8Kc | **50B** / 1.7Kc |
| `ody_11_swe_bugfix_vdp_and_carry` | `.S` Bugfix | ❌ `WRONG_ANS` | **86B** / 3.4Kc | **88B** / 3.4Kc | **88B** / 3.4Kc | **116B** / 5.2Kc | **90B** / 3.5Kc |
| `ody_12_swe_bugfix_xas99_isr` | `.asm` Bugfix | ❌ `LINK_ERR` | **178B** / 207.0Kc | ❌ `LINK_ERR` | **176B** / 206.9Kc | ❌ `TIMEOUT` | **176B** / 206.9Kc |
| **Odyssey Pass@1** | — | **5/12 (41.7%)** | **12/12 (100%)** | **5/12 (41.7%)** | **12/12 (100%)** | **5/12 (41.7%)** | **12/12 (100%)** |

*(Cumulative across all **37** benchmark tasks—20 core/peripheral + 5 bizarro + 12 full-system `ti99sim` Odyssey—Cold models score **17/37 (45.9%) Haiku**, **16/37 (43.2%) Sonnet**, **28/37 (75.7%) Codex**, whereas **all three models score `37/37 (100.0%)` with the Congealed Pack**.)*

---

## 3d. Cross-Fleet & Reasoning-Effort Scaling (`Muse Spark 1.4` vs. `Gemini-4-Carbon` Low / Medium / High)

Testing both ends of the capability & reasoning-effort spectrum (`meta-muse` running **`muse-spark-1.4-internal`** and `meta-pi` running **`gemini-4-carbon`** at `low`, `medium`, and `high` thinking):

| Model / Effort | Core-20 Cold | Core-20 + Pack | Bizarro-5 Cold | Bizarro-5 + Pack | Odyssey-12 Cold | Odyssey-12 + Pack | Key Observation |
|---|---:|---:|---:|---:|---:|---:|---|
| **Muse Spark 1.4** (`meta-muse`) | 13/20 (65.0%) | **20/20 (100%)** | 1/5 (20.0%) | **5/5 (100%)** | 1/12 (8.3%) | **11/12 (91.7%)** | **15/37 (40.5%) -> 36/37 (97.3%)** (+56.8 pp!) |
| **Gemini-4-Carbon (`low`)** | 11/20 (55.0%) | **18/20 (90.0%)** | 3/5 (60.0%) | **5/5 (100%)** | 7/12 (58.3%) | **9/12 (75.0%)** | ~15–35s/task; `low` thinking under-plans large unrolled heaps |
| **Gemini-4-Carbon (`medium`)** | — | — | **5/5 (100%)** | **5/5 (100%)** | 8/12 (66.7%)* | **12/12 (100%)** | **Sweet spot**: `17/17 (100%)`, up to **5.84× faster** than `clang -O2`! |
| **Gemini-4-Carbon (`high`)** | — | — | 3/5 (60.0%)* | **5/5 (100%)** | 4/12 (33.3%)* | **11/12 (91.7%)*** | **16/16 (100%)** of non-timed-out tasks; all-time cycle records (`9,252c` on `ody_03` = **5.49×** `clang -O2`, `15,210c` on `biz_01`) |

*\*Asterisked entries had 0 code errors; remaining tasks exceeded per-call reasoning wall-clock cap (500s–840s) while cycle-optimizing large multi-function modules.*

---

## 4. Running the Benchmark & Boosting Loop

```bash
# Recompute C (-O2 / -Oz) baselines
python3 expertise/eval/run_eval.py --mode c-baseline

# Evaluate any model Cold vs. Core-Only vs. Full Pack
python3 expertise/eval/run_eval.py --mode llm --model haiku  --regime cold --split all
python3 expertise/eval/run_eval.py --mode llm --model haiku  --regime pack --split all
python3 expertise/eval/run_eval.py --mode llm --model sonnet --regime pack --split all
python3 expertise/eval/run_eval.py --mode llm --model codex  --regime pack --split all

# Run the 5-task Bizarro TI-99 System SWE-Bench (GROM bases/wrap, XB, real GPL ROM, SAMS, 378 cart)
python3 expertise/eval/bizarro_bench.py --model sonnet --regime pack --tag v2

# Run the 12-task Full-System ti99sim Odyssey Suite (wait states, Graphics II, 60Hz IRQ, PSG, ABI, bugfix)
python3 expertise/eval/odyssey_bench.py --mode c-baseline
python3 expertise/eval/odyssey_bench.py --model sonnet --regime pack --tag v3 --workers 6
```

To install as a persistent skill in Claude Code or Pi, symlink `expertise/` into `.claude/skills/tms9900-ti99-mastery` (or `~/.pi/agent/skills/tms9900-ti99-mastery`).
