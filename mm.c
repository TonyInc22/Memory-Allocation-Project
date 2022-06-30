/*
 * mm.c
 *
 * Name: Anthony Incorvati, Seth Parker, Omar Darras
 *
 * A heap memory management system for an OS 
 * The main functions are mm init, malloc, free, and realloc 
 * mm init allocates and extends the heap with padding 
 * malloc 
 *
 */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <stdint.h>

#include "mm.h"
#include "memlib.h"

/*
 * If you want to enable your debugging output and heap checker code,
 * uncomment the following line. Be sure not to have debugging enabled
 * in your final submission.
 */
// #define DEBUG

#ifdef DEBUG
/* When debugging is enabled, the underlying functions get called */
#define dbg_printf(...) printf(__VA_ARGS__)
#define dbg_assert(...) assert(__VA_ARGS__)
#else
/* When debugging is disabled, no code gets generated */
#define dbg_printf(...)
#define dbg_assert(...)
#endif /* DEBUG */

/* do not change the following! */
#ifdef DRIVER
/* create aliases for driver tests */
#define malloc mm_malloc
#define free mm_free
#define realloc mm_realloc
#define calloc mm_calloc
#define memset mem_memset
#define memcpy mem_memcpy
#endif /* DRIVER */

/* What is the correct alignment? */
#define ALIGNMENT 16


//basic size constants (Computer Sytems, A Programmer's Perspective)
size_t WSIZE = 8;  /* Word and header/footer size (bytes) */
size_t DSIZE = 16;   /* double Word and header/footer size (bytes) */
int CHUNKSIZE = (1<<12); /* Extend heap by this amount (bytes) ----- WHY this arithmatic?? */
static char *heap_list = 0;



uint64_t MAX(int x, int y)
{
    return ((x) > (y) ? (x) : (y));
}

uint64_t PACK(int size,  int alloc) //Pack size and allocated bit to a word
{
    return ((size) | (alloc));
}
 
uint64_t PUT(char *p, uint64_t val) //write word at address p
{
    return (*(uint64_t *)(p) = (uint64_t)(val));
} 

uint64_t GET(char *p) //reads word at address p
{
    return (*(unsigned int *)(p));
} 

//  return the size from header or footer at address 
uint64_t GET_SIZE(char *p) 
{
    return (GET(p) & ~(DSIZE - 1));
}
// return the allocated bit from header or footer at address 
uint64_t GET_ALLOC(char *p) 
{
    return (GET(p) & 0x1);
}
    

char *HDRP(void *bp) //return header pointer for bp
{
    return (char*)(bp) - WSIZE;
}

char *FTRP(char *bp) //return footer pointer bp
{
    return ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE); 
} 


// return the block pointers of the next block
char *NEXT_BLKP(void *bp) 
{
    return ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE)));
}

// return the block pointers of the previous block

char *PREV_BLKP(void *bp)
{
    return ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE)));
} 

// return next pointer



/* rounds up to the nearest multiple of ALIGNMENT */
static size_t align(size_t x)
{
    return ALIGNMENT * ((x+ALIGNMENT-1)/ALIGNMENT);
}

void *find_fit(size_t fitsize)
{
    void *bp;

    for (bp = heap_list; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp)) 
    {
        if (!GET_ALLOC(HDRP(bp)) && (fitsize <= GET_SIZE(HDRP(bp))))  
        {
            return bp;
        }
    }
    return NULL;
}

void place(void *bp, size_t asize)
{
    size_t csize = GET_SIZE(HDRP(bp));

    if ((csize - asize) >= (2*DSIZE)) 
    {
        PUT(HDRP(bp), PACK(asize, 1));
        PUT(FTRP(bp), PACK(asize, 1));
        bp = NEXT_BLKP(bp);
        PUT(HDRP(bp), PACK(csize-asize, 0));
        PUT(FTRP(bp), PACK(csize-asize, 0));
    }
    else
    {
        PUT(HDRP(bp), PACK(csize, 1));
        PUT(FTRP(bp), PACK(csize, 1));
    }
}

void *coalesce(void *bp)
{
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t size = GET_SIZE(HDRP(bp));

    if (prev_alloc && next_alloc) { /* Case 1 */
        return bp;
    }

    else if (prev_alloc && !next_alloc) { /* Case 2 */
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size,0));
    }

    else if (!prev_alloc && next_alloc) { /* Case 3 */
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        PUT(FTRP(bp), PACK(size, 0));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }
 
    else { /* Case 4 */
        size += GET_SIZE(HDRP(PREV_BLKP(bp))) +
        GET_SIZE(FTRP(NEXT_BLKP(bp)));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }
    return bp;
}
void *extend_heap(size_t words)
{
    char *bp;
    

    /* Allocate an even number of words to maintain alignment */
    if ((long)(bp = mem_sbrk(words)) == -1)
    return NULL;

    /* Initialize free block header/footer and the epilogue header */
    PUT(HDRP(bp), PACK(words, 0)); /* Free block header */
    PUT(FTRP(bp), PACK(words, 0)); /* Free block footer */
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1)); /* New epilogue header */

    /* Coalesce if the previous block was free */
    return coalesce(bp);
 }

/*
 * Initialize: returns false on error, true on success.
 */
bool mm_init(void)
{
    

    /* Create the initial empty heap */
    if ((heap_list = mem_sbrk(4*WSIZE)) == (void *)-1)
    {
        return false;
    }
    PUT(heap_list, 0); /* Alignment padding */
    PUT(heap_list + (1*WSIZE), PACK(DSIZE, 1)); /* Prologue header */
    PUT(heap_list + (2*WSIZE), PACK(DSIZE, 1)); /* Prologue footer */
    PUT(heap_list + (3*WSIZE), PACK(0, 1)); /* Epilogue header */
    heap_list += (2*WSIZE);

    if (extend_heap(CHUNKSIZE/WSIZE) == NULL)
    {
        return false;
    }

    return true;
}

/*
 * malloc
 */
void* malloc(size_t size)
{

    size_t asize; 
    //size_t extendsize; 
    char *bp;

    if (size == 0)
    return NULL;

    asize = align(size)+DSIZE;

    if ((bp = find_fit(asize))!=NULL)
    {
        place(bp,asize);
        return bp;
    }
    
    

    
    if ((bp = extend_heap(asize)) == NULL)
    {
        return NULL;
    }
    place(bp, asize);
    return bp;
    

}


/*
 * free
 */
void free(void* ptr)
{
    size_t size = GET_SIZE(HDRP(ptr));

    PUT(HDRP(ptr), PACK(size, 0)); 
    PUT(FTRP(ptr), PACK(size, 0)); 
    coalesce(ptr);

    return;
}

/*
 * realloc
 */
void* realloc(void* oldptr, size_t size)
{
    /* IMPLEMENT THIS */
    return NULL;
}

/*
 * calloc
 * This function is not tested by mdriver, and has been implemented for you.
 */
void* calloc(size_t nmemb, size_t size)
{
    void* ptr;
    size *= nmemb;
    ptr = malloc(size);
    if (ptr) {
        memset(ptr, 0, size);
    }
    return ptr;
}




static bool in_heap(const void* p)
{
    return p <= mem_heap_hi() && p >= mem_heap_lo();
}

/*
 * Returns whether the pointer is aligned.
 * May be useful for debugging.
 */
static bool aligned(const void* p)
{
    size_t ip = (size_t) p;
    return align(ip) == ip;
}

/*
 * mm_checkheap
 */
bool mm_checkheap(int lineno)
{
#ifdef DEBUG
    /* Write code to check heap invariants here */
    /* IMPLEMENT THIS */
#endif /* DEBUG */
    return true;
}
