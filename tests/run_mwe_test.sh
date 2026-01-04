#!/bin/bash
# Minimum Working Example Test Script
# Tests the TMS9900 LLVM backend with dot product, bubble sort, and verification

set -e

# Configuration - adjust paths as needed
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
LLVM_BUILD="${SCRIPT_DIR}/../llvm-project/build"
XAS99="${XAS99:-python3 ${HOME}/personal/ti99/xdt99/xas99.py}"
TMS9900_TRACE="${TMS9900_TRACE:-${HOME}/personal/ti99/tms9900-trace/build/tms9900-trace}"

# Output directory
OUT_DIR="/tmp/mwe_test"
mkdir -p "$OUT_DIR"

echo "=== TMS9900 LLVM Backend - Minimum Working Example Test ==="
echo ""

# Step 1: Compile C to TMS9900 assembly with xas99 dialect
echo "1. Compiling mwe_test.c to TMS9900 assembly..."
"${LLVM_BUILD}/bin/clang" --target=tms9900 -O2 -S -fno-addrsig \
    -mllvm -tms9900-asm-dialect=xas99 \
    "${SCRIPT_DIR}/mwe_test.c" -o "${OUT_DIR}/mwe_test.s"

# Step 2: Filter LLVM directives (only .text/.p2align remain)
echo "2. Filtering LLVM directives..."
grep -v '^\s*\.' "${OUT_DIR}/mwe_test.s" > "${OUT_DIR}/mwe_clean.s"

# Step 3: Create test harness
echo "3. Creating test harness..."
cat > "${OUT_DIR}/mwe_harness.asm" << 'EOF'
* Minimum Working Example Test Harness
* Expected results:
*   R0 = 0x008D (141 = 55 + 86)
*   result_dot1 = 0x0037 (55)
*   result_dot2 = 0x0056 (86)
*   sorted_array = {1, 2, 5, 8, 9}

       AORG >8000

ENTRY  LWPI >8300
       LI   R10,>83FE
       BL   @compute
HALT   JMP  HALT

EOF
cat "${OUT_DIR}/mwe_clean.s" >> "${OUT_DIR}/mwe_harness.asm"
echo "       END ENTRY" >> "${OUT_DIR}/mwe_harness.asm"

# Step 4: Assemble with xas99
echo "4. Assembling with xas99..."
$XAS99 -R -b "${OUT_DIR}/mwe_harness.asm" -o "${OUT_DIR}/mwe_test.bin"

# Step 5: Run in simulator
echo "5. Running in tms9900-trace simulator..."
echo ""

# Run and capture output
OUTPUT=$("${TMS9900_TRACE}" -l 0x8000 -e 0x8000 -w 0x8300 -n 200 -q \
    -d 0x82CA:40 \
    "${OUT_DIR}/mwe_test.bin" 2>&1)

# Extract R0 from step 145 (the halt loop)
R0=$(echo "$OUTPUT" | grep '"step":145' | sed 's/.*"r":\["\([^"]*\)".*/\1/')

echo "=== Test Results ==="
echo ""
echo "Final R0 value: 0x$R0"
echo ""
echo "Memory layout at 0x82CA:"
echo "$OUTPUT" | grep "Memory dump" -A 3
echo ""

# Verify results
if [ "$R0" = "008D" ]; then
    echo "PASS: R0 = 0x008D (141 = 55 + 86) - Correct!"
    echo ""
    echo "=== Test Complete ==="
    exit 0
else
    echo "FAIL: R0 = 0x$R0 (expected 0x008D)"
    exit 1
fi
