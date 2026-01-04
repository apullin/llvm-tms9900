// Minimum Working Example - TMS9900 LLVM Backend Test
// Tests: dot product, bubble sort, dot product again
// Results stored at fixed memory locations for verification

// Result storage (volatile to prevent optimization)
volatile int result_dot1;    // First dot product result  = 55 (0x37)
volatile int result_dot2;    // Second dot product result = 86 (0x56)
volatile int sorted_array[5]; // Copy of sorted array: {1, 2, 5, 8, 9}

// Input arrays (volatile to force runtime computation)
volatile int input_a[5] = {5, 2, 8, 1, 9};
volatile int input_b[5] = {3, 4, 2, 7, 1};

// Dot product of two 5-element arrays
int dot_product(volatile int *a, volatile int *b, int len) {
    int sum = 0;
    for (int i = 0; i < len; i++) {
        sum += a[i] * b[i];
    }
    return sum;
}

// Bubble sort (ascending order)
void bubble_sort(volatile int *arr, int len) {
    for (int i = 0; i < len - 1; i++) {
        for (int j = 0; j < len - 1 - i; j++) {
            if (arr[j] > arr[j + 1]) {
                int temp = arr[j];
                arr[j] = arr[j + 1];
                arr[j + 1] = temp;
            }
        }
    }
}

// Copy array
void copy_array(volatile int *dst, volatile int *src, int len) {
    for (int i = 0; i < len; i++) {
        dst[i] = src[i];
    }
}

// Main test function - called from harness
int compute(void) {
    // First dot product (unsorted):
    // 5*3 + 2*4 + 8*2 + 1*7 + 9*1 = 15 + 8 + 16 + 7 + 9 = 55
    result_dot1 = dot_product(input_a, input_b, 5);

    // Sort array a
    bubble_sort(input_a, 5);

    // Copy sorted array for verification
    copy_array(sorted_array, input_a, 5);

    // Second dot product (with sorted a):
    // 1*3 + 2*4 + 5*2 + 8*7 + 9*1 = 3 + 8 + 10 + 56 + 9 = 86
    result_dot2 = dot_product(input_a, input_b, 5);

    // Return sum of both results for quick check
    return result_dot1 + result_dot2;  // 55 + 86 = 141 = 0x8D
}
