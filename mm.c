/*
 * mm.c
 *
 * Name: Anthony Incorvati - ari5086, Seth Parker - sdp218, Omar Darras - opd5033
 *
 *
 * A heap memory management system for an OS 
 * The main functions are mm init, malloc, free, and realloc 
 * The mm init function allocates and extends the heap with padding 
 * The malloc function allocates a payload of requested size into the heap with proper alignment and splitting if needed
 * The free function deallocates space at a requested address in the heap, performing coalescing if needed
 * The realloc function reallocates space at a requested address in the heap performing either a free or malloc call depending on the size requested
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

#define ALIGNMENT 16

// Size of chunk, head/foot, and double head/foot
size_t HEAD_SIZE = 8;
size_t DHEAD_SIZE = 16;  

// Initialization of the starting address for the heap
static char *heap_start = 0;

// Uses bitwise operators to return a package of size and allocation ready to be placed into the heap
uint64_t PACK(int size,  int alloc)
{
    return ((size) | (alloc));
}

//Writes val at given address
uint64_t PUT(char *addr, uint64_t val) 
{
    return (*(uint64_t *)(addr) = (uint64_t)(val));
} 

//Reads at given address and returns package
uint64_t GET(char *addr)
{
    return (*(unsigned int *)(addr));
} 

//  return the size from header or footer at address 
uint64_t GET_SIZE(char *addr) 
{
    return (GET(addr) & ~(DHEAD_SIZE - 1));
}
// return the allocated bit from header or footer at address 
uint64_t GET_ALLOC(char *addr) 
{
    return (GET(addr) & 0x1);
}
    
// return address to the header
char *HDRP(void *addr) 
{
    return (char*)(addr) - HEAD_SIZE;
}

// return address to the footer
char *FTRP(char *addr) 
{
    return ((char *)(addr) + GET_SIZE(HDRP(addr)) - DHEAD_SIZE); 
} 


// return the address of the next block
char *NEXT_ADDR(void *addr) 
{
    return ((char *)(addr) + GET_SIZE(((char *)(addr) - HEAD_SIZE)));
}

// return the address of the previous block

char *PREV_ADDR(void *addr)
{
    return ((char *)(addr) - GET_SIZE(((char *)(addr) - DHEAD_SIZE)));
} 

/* rounds up to the nearest multiple of ALIGNMENT */
static size_t align(size_t x)
{
    return ALIGNMENT * ((x+ALIGNMENT-1)/ALIGNMENT);
}

// First fit algorithm to find the first available space in the heap for the given size
void *find_fit(size_t size)
{
    void *addr;

    for (addr = heap_start; GET_SIZE(HDRP(addr)) > 0; addr = NEXT_ADDR(addr)) 
    {
        if (!GET_ALLOC(HDRP(addr)) && (size <= GET_SIZE(HDRP(addr))))  
        {
            return addr;
        }
    }
    return NULL;
}

// Places a header and footer into the heap at a given address and deals with splitting if necessary
void place(void *addr, size_t new_size)
{
    size_t old_size = GET_SIZE(HDRP(addr));

    //If splitting is necessary
    if ((old_size - new_size) >= (2*DHEAD_SIZE)) 
    {
        PUT(HDRP(addr), PACK(new_size, 1));
        PUT(FTRP(addr), PACK(new_size, 1));
        addr = NEXT_ADDR(addr);
        PUT(HDRP(addr), PACK(old_size-new_size, 0));
        PUT(FTRP(addr), PACK(old_size-new_size, 0));
    }
    //If splitting is not necessary
    else
    {
        PUT(HDRP(addr), PACK(old_size, 1));
        PUT(FTRP(addr), PACK(old_size, 1));
    }
}

//Coalesces free blocks at given address
void *coalesce(void *addr)
{
    size_t prev = GET_ALLOC(FTRP(PREV_ADDR(addr)));
    size_t next = GET_ALLOC(HDRP(NEXT_ADDR(addr)));
    size_t size = GET_SIZE(HDRP(addr));

    // CASE 1: No coalescing needed
    if (prev && next) { 
        return addr;
    }

    // CASE 2: Coalesce the next block
    else if (prev && !next) {
        size += GET_SIZE(HDRP(NEXT_ADDR(addr)));
        PUT(HDRP(addr), PACK(size, 0));
        PUT(FTRP(addr), PACK(size,0));
    }

    // CASE 3: Coalesce the previous block
    else if (!prev && next) { 
        size += GET_SIZE(HDRP(PREV_ADDR(addr)));
        PUT(FTRP(addr), PACK(size, 0));
        PUT(HDRP(PREV_ADDR(addr)), PACK(size, 0));
        addr = PREV_ADDR(addr);
    }
 
    // CASE 4: Coalesce both blocks
    else { 
        size += GET_SIZE(HDRP(PREV_ADDR(addr))) +
        GET_SIZE(FTRP(NEXT_ADDR(addr)));
        PUT(HDRP(PREV_ADDR(addr)), PACK(size, 0));
        PUT(FTRP(NEXT_ADDR(addr)), PACK(size, 0));
        addr = PREV_ADDR(addr);
    }
    return addr;
}

// Extends the heap using mem_sbrk function of given size
void *extend_heap(size_t size)
{
    char *addr;

    // Allocate an even number of words to maintain alignment 
    if ((long)(addr = mem_sbrk(size)) == -1)
    return NULL;

    // Initialize free block header/footer and the buffer header 
    PUT(HDRP(addr), PACK(size, 0));  
    PUT(FTRP(addr), PACK(size, 0)); 
    PUT(HDRP(NEXT_ADDR(addr)), PACK(0, 1)); 

    mm_checkheap(__LINE__);

    // Coalesce if the previous block was free 
    return coalesce(addr);
 }

/*
 * Initialize: returns false on error, true on success.
 */
bool mm_init(void)
{
    
    /* Create the initial empty heap */
    if ((heap_start = mem_sbrk(4*HEAD_SIZE)) == (void *)-1)
    {
        return false;
    }

    // Create padding/buffer, header, and footer into heap for intitialization
    PUT(heap_start, 0); 
    PUT(heap_start + (1*HEAD_SIZE), PACK(DHEAD_SIZE, 1)); 
    PUT(heap_start + (2*HEAD_SIZE), PACK(DHEAD_SIZE, 1)); 
    PUT(heap_start + (3*HEAD_SIZE), PACK(0, 1)); 
    heap_start += (2*HEAD_SIZE);

    if (extend_heap((1<<12)/HEAD_SIZE) == NULL)
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
    char *addr;

    if (size == 0)
    return NULL;

    asize = align(size)+DHEAD_SIZE;

    // If there is a fit in the heap, place a new header and footer there
    if ((addr = find_fit(asize)) != NULL)
    {
        place(addr,asize);
        return addr;
    }
    
    // Request more room in the heap and check to make sure it's granted
    if ((addr = extend_heap(asize)) == NULL)
    {
        return NULL;
    }

    // Place header and footer at new address in the heap
    place(addr, asize);
    return addr;
    

}


/*
 * free
 */
void free(void* ptr)
{
    size_t size = GET_SIZE(HDRP(ptr));

    // Put a header and footer at the given address containing a pack of size and allocation boolean
    PUT(HDRP(ptr), PACK(size, 0)); 
    PUT(FTRP(ptr), PACK(size, 0)); 

    // Run new free block address through coalesce function to check if coalecsing is necessary
    coalesce(ptr);

    return;
}

/*
 * realloc
 */
void* realloc(void* oldptr, size_t size)
{

    // Check if given parameters are valid
    if (size == 0)
    {
        free(oldptr);
        return NULL;
    }
    else if (oldptr == NULL)
    {
        char *addr = malloc(size);
        return addr;
    }

    else
    {
        size_t old_size = GET_SIZE(HDRP(oldptr));
        size_t new_size = align(size) +DHEAD_SIZE ;

        // Perform malloc and free operation if given size is less than the existing one at given address
        if(old_size < new_size)
        {
            void *new_ptr = malloc(new_size);
            memcpy(new_ptr, oldptr, old_size);
            free(oldptr);
            return new_ptr; 
        }
        else
        {   
            return oldptr;
        }
    }
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
bool mm_checkheap(int lineno) {

    void *addr = heap_start;

    dbg_printf("---------------- MM CHECK HEAP %d ----------------\n", lineno);
    dbg_printf("     Mem Heap Lo|     Mem Heap Hi|\n");
    dbg_printf("%16lx|%16lx|\n", (uint64_t)(mem_heap_lo() - mem_heap_lo()), (uint64_t)(mem_heap_hi() - mem_heap_lo()));

    int count = 0;
    while(addr < mem_heap_hi() - DHEAD_SIZE){
        dbg_printf("---------------------------------------------------\n");
        dbg_printf("%6d Head Size|       Head Free|    Head Address|\n", count);
        dbg_printf("%16lx|%16lx|%16lx|\n", GET_SIZE(HDRP(addr)), GET_ALLOC(HDRP(addr)), GET(HDRP(addr)));        
        // dbg_printf("       Foot Size|       Foot Free|    Foot Address|\n");
        // dbg_printf("%16lx|%16lx|%16lx|\n", GET_SIZE(FTRP(addr)), GET_ALLOC(FTRP(addr)), GET(FTRP(addr)));  

        count += 1;
        addr = NEXT_ADDR(addr);
    }
    return true;

}