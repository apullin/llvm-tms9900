* Minimum Working Example Test Harness
* Expected results after execution:
*   R0 = 0x008D (141 = 55 + 86)
*   result_dot1 = 0x0037 (55)
*   result_dot2 = 0x0056 (86)
*   sorted_array = {1, 2, 5, 8, 9}

       AORG >8000

* Entry point - set up workspace and stack
ENTRY  LWPI >8300         ; Workspace at >8300
       LI   R10,>83FE     ; Stack pointer just below workspace
       BL   @compute      ; Call main test function
HALT   JMP  HALT          ; Halt - result in R0

* Include the compiled test code
       COPY "/tmp/mwe_fixed.s"

       END ENTRY
