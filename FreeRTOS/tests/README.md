Run the optional CPU execution tests after building Clang, LLD, and the LLVM
object utilities:

```sh
python3 FreeRTOS/tests/run_tests.py --trace /path/to/tms9900-trace
```

The default matrix uses `-O0`, `-O1`, `-O2`, `-Os`, and `-Oz`. Tests check:

- task workspace and initial context alignment for both word-aligned stack-top
  residues, initial registers/arguments, and canaries outside the frame;
- IR memory/status clobbers on interrupt-mask changes, plus actual assembly;
- byte-exact BSS initialization at odd addresses and across `0x8000`, including
  empty sections and canaries on both sides of BSS;
- linker rejection of an image that would overwrite reserved memory;
- both complete demos with timer interrupts, software yields and queue waits.

The startup fixtures retain the vector table so binary address zero corresponds
to the linker's address zero. The runner poisons BSS before execution; relying
on initially zero simulator RAM would hide startup errors. `--startup PATH`
permits comparisons with old startup assembly.

The demo linker map reserves the ISR workspace/stack, result markers, main
workspace and a 4 KiB main stack. Program storage is limited to `0x0100–0x7EFF`.
This flat CPU simulator layout does not describe TI-99/4A hardware.
