/* heap4 benchmark — FreeRTOS heap_4-style allocator for TMS9900
 *
 * Implements a simplified version of FreeRTOS's heap_4 allocator:
 *   - Static 2048-byte heap buffer
 *   - Free list with linked list of free blocks
 *   - First-fit allocation
 *   - Block splitting when free block is larger than needed
 *   - Coalescing of adjacent free blocks on free
 *
 * Exercises:
 *   - Pointer chasing (linked list traversal)
 *   - Struct field access (BlockLink_t members)
 *   - Comparison-heavy control flow (size checks, NULL checks)
 *   - Mixed pointer arithmetic (block addresses from headers)
 *   - 16-bit pointer math throughout
 */
#include <stdint.h>

__attribute__((noinline)) static void halt_ok(void) { __asm__ volatile("idle"); }
__attribute__((noinline)) static void fail_loop(void) { for (;;) {} }

/* ---- Heap allocator ---- */

#define HEAP_SIZE         2048u
#define ALIGNMENT         2u
#define ALIGNMENT_MASK    (ALIGNMENT - 1u)

typedef struct BlockLink {
    struct BlockLink *pxNextFreeBlock;
    unsigned int      xBlockSize;     /* includes header size */
} BlockLink_t;

#define HEADER_SIZE  ((unsigned int)sizeof(BlockLink_t))

/* Minimum allocatable block: header + at least 2 bytes payload */
#define MIN_BLOCK_SIZE  (HEADER_SIZE + ALIGNMENT)

/* The heap buffer, 2-byte aligned */
static uint8_t ucHeap[HEAP_SIZE] __attribute__((aligned(2)));

/* Sentinel nodes: xStart is the head, xEnd marks the tail */
static BlockLink_t xStart;
static BlockLink_t xEnd;

static unsigned int xFreeBytesRemaining;

/* Insert a free block into the sorted-by-address free list */
__attribute__((noinline))
static void prvInsertBlockIntoFreeList(BlockLink_t *pxBlockToInsert) {
    BlockLink_t *pxIterator;

    /* Walk the list to find the block just before where this one goes
     * (list is sorted by address for coalescing) */
    for (pxIterator = &xStart;
         pxIterator->pxNextFreeBlock < pxBlockToInsert;
         pxIterator = pxIterator->pxNextFreeBlock) {
        /* Nothing — just walk */
    }

    /* Coalesce with the block before? */
    {
        uint8_t *puc = (uint8_t *)pxIterator;
        if (puc + pxIterator->xBlockSize == (uint8_t *)pxBlockToInsert) {
            /* Merge: absorb pxBlockToInsert into pxIterator */
            pxIterator->xBlockSize += pxBlockToInsert->xBlockSize;
            pxBlockToInsert = pxIterator;
        }
    }

    /* Coalesce with the block after? */
    {
        uint8_t *puc = (uint8_t *)pxBlockToInsert;
        if (puc + pxBlockToInsert->xBlockSize ==
            (uint8_t *)pxIterator->pxNextFreeBlock) {
            if (pxIterator->pxNextFreeBlock != &xEnd) {
                /* Merge: absorb the next block */
                pxBlockToInsert->xBlockSize +=
                    pxIterator->pxNextFreeBlock->xBlockSize;
                pxBlockToInsert->pxNextFreeBlock =
                    pxIterator->pxNextFreeBlock->pxNextFreeBlock;
            } else {
                pxBlockToInsert->pxNextFreeBlock = &xEnd;
            }
        } else {
            pxBlockToInsert->pxNextFreeBlock = pxIterator->pxNextFreeBlock;
        }
    }

    /* Link in (unless merged into pxIterator) */
    if (pxIterator != pxBlockToInsert) {
        pxIterator->pxNextFreeBlock = pxBlockToInsert;
    }
}

__attribute__((noinline))
static void heap_init(void) {
    BlockLink_t *pxFirstFreeBlock;
    unsigned int uxAddress;

    /* Align the start of the heap */
    uxAddress = (unsigned int)ucHeap;
    if (uxAddress & ALIGNMENT_MASK) {
        uxAddress += ALIGNMENT;
        uxAddress &= ~ALIGNMENT_MASK;
    }

    /* xStart: size 0, points to the first real free block */
    xStart.xBlockSize = 0;

    /* Place xEnd at the end of the usable heap area */
    uxAddress = (unsigned int)ucHeap + HEAP_SIZE;
    uxAddress -= HEADER_SIZE;
    uxAddress &= ~ALIGNMENT_MASK;
    xEnd.xBlockSize = 0;
    xEnd.pxNextFreeBlock = (BlockLink_t *)0;

    /* First free block spans the entire usable heap */
    pxFirstFreeBlock = (BlockLink_t *)((unsigned int)ucHeap);
    pxFirstFreeBlock->xBlockSize = uxAddress - (unsigned int)ucHeap;
    pxFirstFreeBlock->pxNextFreeBlock = &xEnd;

    xStart.pxNextFreeBlock = pxFirstFreeBlock;
    xFreeBytesRemaining = pxFirstFreeBlock->xBlockSize;
}

__attribute__((noinline))
static void *heap_malloc(unsigned int xWantedSize) {
    BlockLink_t *pxBlock;
    BlockLink_t *pxPreviousBlock;
    BlockLink_t *pxNewBlockLink;
    void *pvReturn = (void *)0;

    if (xWantedSize == 0) return (void *)0;

    /* Add header size and align up */
    xWantedSize += HEADER_SIZE;
    if (xWantedSize & ALIGNMENT_MASK) {
        xWantedSize += ALIGNMENT;
        xWantedSize &= ~ALIGNMENT_MASK;
    }

    if (xWantedSize <= xFreeBytesRemaining) {
        /* First-fit search */
        pxPreviousBlock = &xStart;
        pxBlock = xStart.pxNextFreeBlock;

        while ((pxBlock->xBlockSize < xWantedSize) &&
               (pxBlock->pxNextFreeBlock != (BlockLink_t *)0)) {
            pxPreviousBlock = pxBlock;
            pxBlock = pxBlock->pxNextFreeBlock;
        }

        if (pxBlock != &xEnd) {
            /* Found a suitable block */
            pvReturn = (void *)((uint8_t *)pxBlock + HEADER_SIZE);

            /* Remove from free list */
            pxPreviousBlock->pxNextFreeBlock = pxBlock->pxNextFreeBlock;

            /* Split if the remaining space is large enough */
            if ((pxBlock->xBlockSize - xWantedSize) >= MIN_BLOCK_SIZE) {
                pxNewBlockLink =
                    (BlockLink_t *)((uint8_t *)pxBlock + xWantedSize);
                pxNewBlockLink->xBlockSize =
                    pxBlock->xBlockSize - xWantedSize;
                pxBlock->xBlockSize = xWantedSize;

                /* Insert remainder into free list */
                prvInsertBlockIntoFreeList(pxNewBlockLink);
            }

            xFreeBytesRemaining -= pxBlock->xBlockSize;
            /* Clear next pointer to mark as allocated */
            pxBlock->pxNextFreeBlock = (BlockLink_t *)0;
        }
    }

    return pvReturn;
}

__attribute__((noinline))
static void heap_free(void *pv) {
    BlockLink_t *pxLink;

    if (pv == (void *)0) return;

    /* Step back to the header */
    pxLink = (BlockLink_t *)((uint8_t *)pv - HEADER_SIZE);

    xFreeBytesRemaining += pxLink->xBlockSize;

    prvInsertBlockIntoFreeList(pxLink);
}

__attribute__((noinline))
static unsigned int heap_get_free(void) {
    return xFreeBytesRemaining;
}

/* ---- Checksum helpers ---- */

/* Rotate-XOR: accumulates a 16-bit value into the running checksum */
__attribute__((noinline))
static unsigned int rxor(unsigned int ck, unsigned int val) {
    ck = ((ck << 3) | (ck >> 13)) ^ val;
    return ck;
}

/* Convert a heap pointer to a position-independent offset from the heap base.
 * This ensures the checksum does not depend on absolute addresses. */
__attribute__((noinline))
static unsigned int heap_offset(void *p) {
    if (p == (void *)0) return 0xFFFFu;  /* sentinel for NULL */
    return (unsigned int)((uint8_t *)p - ucHeap);
}

/* ---- Test workload ---- */

volatile unsigned int result;

int main(void) {
    unsigned int ck = 0;

    heap_init();

    /* Record initial state */
    ck = rxor(ck, heap_get_free());

    /* ===== Phase 1: Basic allocation and data storage ===== */
    {
        void *p1, *p2, *p3, *p4;
        uint16_t *d;

        p1 = heap_malloc(8);    /* small */
        p2 = heap_malloc(32);   /* medium */
        p3 = heap_malloc(128);  /* large */
        p4 = heap_malloc(4);    /* tiny */

        ck = rxor(ck, heap_offset(p1));
        ck = rxor(ck, heap_offset(p2));
        ck = rxor(ck, heap_offset(p3));
        ck = rxor(ck, heap_offset(p4));

        /* Write data into allocations */
        d = (uint16_t *)p1;
        d[0] = 0xDEAD; d[1] = 0xBEEF;
        d = (uint16_t *)p2;
        d[0] = 0x1234; d[7] = 0x5678;
        d = (uint16_t *)p3;
        d[0] = 0xCAFE; d[31] = 0xBABE;
        d = (uint16_t *)p4;
        d[0] = 0xF00D;

        /* Read back and checksum */
        d = (uint16_t *)p1;
        ck = rxor(ck, d[0]); ck = rxor(ck, d[1]);
        d = (uint16_t *)p2;
        ck = rxor(ck, d[0]); ck = rxor(ck, d[7]);
        d = (uint16_t *)p3;
        ck = rxor(ck, d[0]); ck = rxor(ck, d[31]);
        d = (uint16_t *)p4;
        ck = rxor(ck, d[0]);

        ck = rxor(ck, heap_get_free());

        /* Free in LIFO order */
        heap_free(p4);
        heap_free(p3);
        heap_free(p2);
        heap_free(p1);

        ck = rxor(ck, heap_get_free());
    }

    /* ===== Phase 2: FIFO free order ===== */
    {
        void *ptrs[6];
        unsigned int i;

        for (i = 0; i < 6; i++) {
            ptrs[i] = heap_malloc(16);
            ck = rxor(ck, heap_offset(ptrs[i]));
        }

        ck = rxor(ck, heap_get_free());

        /* Free in FIFO order */
        for (i = 0; i < 6; i++) {
            heap_free(ptrs[i]);
        }

        ck = rxor(ck, heap_get_free());
    }

    /* ===== Phase 3: Coalescing verification ===== */
    {
        void *a, *b, *c, *big;

        a = heap_malloc(32);
        b = heap_malloc(32);
        c = heap_malloc(32);

        ck = rxor(ck, heap_offset(a));
        ck = rxor(ck, heap_offset(b));
        ck = rxor(ck, heap_offset(c));

        /* Free a and b (adjacent) — they should coalesce */
        heap_free(a);
        heap_free(b);

        ck = rxor(ck, heap_get_free());

        /* Now allocate something that fits in the coalesced a+b space */
        big = heap_malloc(60);
        ck = rxor(ck, heap_offset(big));
        ck = rxor(ck, heap_get_free());

        /* If coalescing failed, big would come from elsewhere or fail */

        heap_free(big);
        heap_free(c);

        ck = rxor(ck, heap_get_free());
    }

    /* ===== Phase 4: Fragmentation test ===== */
    {
        void *slots[8];
        unsigned int i;
        void *med;

        /* Allocate 8 small blocks */
        for (i = 0; i < 8; i++) {
            slots[i] = heap_malloc(8);
            ck = rxor(ck, heap_offset(slots[i]));
        }

        ck = rxor(ck, heap_get_free());

        /* Free every other one (0, 2, 4, 6) — creates fragmentation */
        for (i = 0; i < 8; i += 2) {
            heap_free(slots[i]);
        }

        ck = rxor(ck, heap_get_free());

        /* Try to allocate a medium block — should NOT fit in the gaps
         * (each gap is only 12 bytes = 8 payload + 4 header) */
        med = heap_malloc(20);
        ck = rxor(ck, heap_offset(med));
        ck = rxor(ck, heap_get_free());

        /* Free remaining odd slots */
        for (i = 1; i < 8; i += 2) {
            heap_free(slots[i]);
        }

        if (med) heap_free(med);

        ck = rxor(ck, heap_get_free());
    }

    /* ===== Phase 5: Stress with mixed sizes ===== */
    {
        void *pa, *pb, *pc, *pd, *pe;
        uint16_t *d;

        pa = heap_malloc(6);
        pb = heap_malloc(64);
        pc = heap_malloc(10);
        pd = heap_malloc(128);
        pe = heap_malloc(4);

        ck = rxor(ck, heap_offset(pa));
        ck = rxor(ck, heap_offset(pb));
        ck = rxor(ck, heap_offset(pc));
        ck = rxor(ck, heap_offset(pd));
        ck = rxor(ck, heap_offset(pe));

        /* Write patterns */
        d = (uint16_t *)pa; d[0] = 0x1111;
        d = (uint16_t *)pb; d[0] = 0x2222; d[15] = 0x3333;
        d = (uint16_t *)pc; d[0] = 0x4444;
        d = (uint16_t *)pd; d[0] = 0x5555; d[31] = 0x6666;
        d = (uint16_t *)pe; d[0] = 0x7777;

        /* Checksum data */
        d = (uint16_t *)pa; ck = rxor(ck, d[0]);
        d = (uint16_t *)pb; ck = rxor(ck, d[0]); ck = rxor(ck, d[15]);
        d = (uint16_t *)pc; ck = rxor(ck, d[0]);
        d = (uint16_t *)pd; ck = rxor(ck, d[0]); ck = rxor(ck, d[31]);
        d = (uint16_t *)pe; ck = rxor(ck, d[0]);

        /* Free in a scrambled order: c, a, e, d, b */
        heap_free(pc);
        heap_free(pa);
        heap_free(pe);
        heap_free(pd);
        heap_free(pb);

        ck = rxor(ck, heap_get_free());
    }

    /* ===== Phase 6: Full reuse — verify entire heap is available ===== */
    {
        void *huge;
        unsigned int free_before;

        free_before = heap_get_free();
        ck = rxor(ck, free_before);

        /* Allocate almost the entire heap
         * (max usable is total - one header for the allocated block) */
        huge = heap_malloc(free_before - HEADER_SIZE);
        ck = rxor(ck, heap_offset(huge));
        ck = rxor(ck, heap_get_free());

        if (huge) {
            /* Write endpoints */
            uint16_t *d = (uint16_t *)huge;
            unsigned int nwords =
                (free_before - HEADER_SIZE) / sizeof(uint16_t);
            d[0] = 0xAAAA;
            d[nwords - 1] = 0x5555;
            ck = rxor(ck, d[0]);
            ck = rxor(ck, d[nwords - 1]);

            heap_free(huge);
        }

        ck = rxor(ck, heap_get_free());
    }

    /* ===== Phase 7: Rapid alloc/free cycles ===== */
    {
        unsigned int i;
        for (i = 0; i < 16; i++) {
            void *p = heap_malloc(8 + (i & 7u) * 4);
            ck = rxor(ck, heap_offset(p));
            if (p) {
                uint16_t *d = (uint16_t *)p;
                d[0] = (uint16_t)(i * 0x1337u);
                ck = rxor(ck, d[0]);
                heap_free(p);
            }
        }
        ck = rxor(ck, heap_get_free());
    }

    result = ck;

    if (ck == 0x0711u)
        halt_ok();

    fail_loop();
    return 0;
}
