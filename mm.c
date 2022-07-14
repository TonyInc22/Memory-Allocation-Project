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
//  #define DEBUG

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
static char *heap_start;

static char *free_list_lo, *free_list_hi;

// Uses bitwise operators to return a package of size and allocation ready to be placed into the heap
uint64_t PACK(size_t size,  size_t alloc)
{
    // if ((uint64_t)size > 0xFFFFFFFF) printf("\n\nTEST\n\n");
    return ((size) | (alloc));
}

//Writes val at given address
void PUT(char *addr, uint64_t val) 
{
    *(uint64_t *)(addr) = (uint64_t)(val);
    return;
}

void PUT_FREELIST(char *addr, char *prev, char *next)
{
    PUT(addr, (uint64_t)prev);
    PUT(addr + HEAD_SIZE, (uint64_t)next);
}

//Reads at given address and returns package
uint64_t GET(char *addr)
{
    return (*(uint64_t *)(addr));
} 

//  return the size from header or footer at address 
size_t GET_SIZE(char *addr) 
{
    return (GET(addr) & ~(DHEAD_SIZE - 1));
}

// return the allocated bit from header or footer at address 
size_t GET_ALLOC(char *addr) 
{
    return (GET(addr) & 0x1);
}

char *GET_PREV_FREE(char *addr)
{
    return (char *)(GET(addr));
}

char *GET_NEXT_FREE(char *addr)
{
    return (char *)(GET(addr + HEAD_SIZE));
}

// return address to the header
char *HEADER(char *addr) 
{
    return (char*)(addr) - HEAD_SIZE;
}

// return address to the footer
char *FOOTER(char *addr) 
{
    return ((char *)(addr) + GET_SIZE(HEADER(addr)) - DHEAD_SIZE); 
}

// return the address of the next block
char *NEXT_ADDR(char *addr) 
{
    return ((char *)(addr) + GET_SIZE(((char *)(addr) - HEAD_SIZE)));
}

// return the address of the previous block
char *PREV_ADDR(char *addr)
{
    return ((char *)(addr) - GET_SIZE(((char *)(addr) - DHEAD_SIZE)));
} 

void NEW_FREELIST(char *addr)
{
    // If this is the start of the free list, set the free list lo and hi pointers
    if (free_list_lo == heap_start) {
        free_list_lo = addr;
        free_list_hi = free_list_lo;
    }

    PUT_FREELIST(addr, free_list_hi, free_list_lo);

    if (free_list_lo != heap_start) {

        //Update first and last entry of freelist
        if (free_list_hi == free_list_lo) PUT_FREELIST(free_list_hi, addr, addr);
        else {
            PUT_FREELIST(free_list_hi, GET_PREV_FREE(free_list_hi), addr);
            PUT_FREELIST(free_list_lo, addr, GET_NEXT_FREE(free_list_lo));
        }

        //Update free list hi index address
        free_list_hi = addr; 
    }
    return;
}

void REMOVE_FREELIST(char *addr)
{
    // The freelist is empty, this function shouldn't have been called
    if (free_list_lo == heap_start) mm_checkheap(__LINE__);

    // There is only one element in the free list
    else if (free_list_lo == free_list_hi) {
        free_list_lo = heap_start;
        free_list_hi = free_list_lo;
    }

    // There are only two elements in the free list
    else if (addr == GET_PREV_FREE(GET_PREV_FREE(addr))) {
        char *prev = GET_PREV_FREE(addr);
        PUT_FREELIST(prev, prev, prev);
        free_list_lo = prev;
        free_list_hi = free_list_lo;
    }

    // General case for more than two elements in the free list
    else {
        PUT_FREELIST(GET_PREV_FREE(addr), GET_PREV_FREE(GET_PREV_FREE(addr)), GET_NEXT_FREE(addr));
        PUT_FREELIST(GET_NEXT_FREE(addr), GET_PREV_FREE(addr), GET_NEXT_FREE(GET_NEXT_FREE(addr)));
        if (addr == free_list_hi) free_list_hi = GET_PREV_FREE(addr);
        if (addr == free_list_lo) free_list_lo = GET_NEXT_FREE(addr); 
    }
    return;
}

/* rounds up to the nearest multiple of ALIGNMENT */
static size_t align(size_t x)
{
    return ALIGNMENT * ((x+ALIGNMENT-1)/ALIGNMENT);
}

// First fit algorithm to find the first available space in the heap using the free list for the given size
void *find_fit(size_t size)
{
    if (free_list_lo == heap_start) return NULL;

    char *addr = free_list_lo;

    do if (!GET_ALLOC(HEADER(addr)) && (size <= GET_SIZE(HEADER(addr)))) return addr;
    while ((addr = GET_NEXT_FREE(addr)) != free_list_lo);

    return NULL;
}

// Places a header and footer into the heap at a given address and deals with splitting if necessary, specifically used by malloc
void place(char *addr, size_t new_size)
{
    size_t old_size = GET_SIZE(HEADER(addr));

    REMOVE_FREELIST(addr);

    //If splitting is necessary
    if ((old_size - new_size) >= (2*DHEAD_SIZE)) 
    {
        PUT(HEADER(addr), PACK(new_size, 1));
        PUT(FOOTER(addr), PACK(new_size, 1));

        addr = NEXT_ADDR(addr);

        PUT(HEADER(addr), PACK(old_size-new_size, 0));
        PUT(FOOTER(addr), PACK(old_size-new_size, 0));

        NEW_FREELIST(addr);
    }
    //If splitting is not necessary
    else
    {
        PUT(HEADER(addr), PACK(old_size, 1));
        PUT(FOOTER(addr), PACK(old_size, 1));
    }
}

//Coalesces free blocks at given address
char *coalesce(char *addr)
{
    size_t prev = GET_ALLOC(FOOTER(PREV_ADDR(addr)));
    size_t next = GET_ALLOC(HEADER(NEXT_ADDR(addr)));
    size_t size = GET_SIZE(HEADER(addr));

    // CASE 1: No coalescing needed
    if (prev && next) {
        return addr;
    }
    // CASE 2: Coalesce the next block
    else if (prev && !next) {
        REMOVE_FREELIST(NEXT_ADDR(addr));

        size += GET_SIZE(HEADER(NEXT_ADDR(addr)));
        PUT(HEADER(addr), PACK(size, 0));
        PUT(FOOTER(addr), PACK(size, 0));
        
    }
    // CASE 3: Coalesce the previous block
    else if (!prev && next) {
        REMOVE_FREELIST(PREV_ADDR(addr));

        size += GET_SIZE(HEADER(PREV_ADDR(addr)));
        PUT(FOOTER(addr), PACK(size, 0));
        PUT(HEADER(PREV_ADDR(addr)), PACK(size, 0));

        addr = PREV_ADDR(addr);
    }
 
    // CASE 4: Coalesce both blocks
    else { 
        REMOVE_FREELIST(PREV_ADDR(addr));
        REMOVE_FREELIST(NEXT_ADDR(addr));

        size += GET_SIZE(HEADER(PREV_ADDR(addr))) +
        GET_SIZE(FOOTER(NEXT_ADDR(addr)));
        PUT(HEADER(PREV_ADDR(addr)), PACK(size, 0));
        PUT(FOOTER(NEXT_ADDR(addr)), PACK(size, 0));

        addr = PREV_ADDR(addr);
    }

    return addr;
}

// Extends the heap using mem_sbrk function of given size
char *extend_heap(size_t size)
{
    char *addr;

    // Allocate an even number of words to maintain alignment 
    if ((long)(addr = mem_sbrk(size)) == -1) return NULL;

    // Initialize free block header/footer and the buffer header 
    PUT(HEADER(addr), PACK(size, 0));  
    PUT(FOOTER(addr), PACK(size, 0)); 
    PUT(HEADER(NEXT_ADDR(addr)), PACK(0, 1)); 

    // Coalesce previous buffer into extended heap
    char *new_addr = coalesce(addr);

    NEW_FREELIST(new_addr);

    return new_addr;
 }

/*
 * Initialize: returns false on error, true on success.
 */
bool mm_init(void)
{
    /* Create the initial empty heap and make sure it is the same address as mem heap lo */
    if ((heap_start = (char *)mem_sbrk(4*HEAD_SIZE)) == (void *)-1) return false;
    if ((uint64_t)heap_start != (uint64_t)mem_heap_lo()) return false;

    // Create padding/buffer, header, and footer into heap for intitialization
    PUT(heap_start, 0); 
    PUT(heap_start + (1*HEAD_SIZE), PACK(DHEAD_SIZE, 1)); 
    PUT(heap_start + (2*HEAD_SIZE), PACK(DHEAD_SIZE, 1)); 
    PUT(heap_start + (3*HEAD_SIZE), PACK(0, 1));

    heap_start += (2*HEAD_SIZE);

    free_list_lo = heap_start;
    free_list_hi = free_list_lo;

    if (extend_heap((1<<12)/HEAD_SIZE) == NULL) return false;

    return true;
}

/*
 * malloc
 */
void* malloc(size_t size)
{
    size_t asize; 
    char *addr;

    if (size == 0) return NULL;

    asize = align(size)+DHEAD_SIZE;

    dbg_printf("\nMALLOC CALL OF SIZE %lx ALIGNED TO %lx", (uint64_t)size, (uint64_t)asize);

    // If there is a fit in the heap, place a new header and footer there
    if ((addr = find_fit(asize)) != NULL)
    {
        place(addr,asize);

        // Check if heap is still correct after placement and display placement address
        dbg_printf(" WAS PLACED AT ADDRESS %lx\n", (uint64_t)addr - (uint64_t)mem_heap_lo());
        if (!mm_checkheap(__LINE__)) return false;

        return addr;
    }
    // Request more room in the heap and check to make sure it's granted
    if ((addr = extend_heap(asize)) == NULL)
    {  
        return NULL;
    }

    // Place header and footer at new address in the heap
    place(addr, asize);

    // Check if heap is still correct after placement and display placement address
    dbg_printf(" WAS PLACED AT ADDRESS %lx\n", (uint64_t)addr - (uint64_t)mem_heap_lo());
    if (!mm_checkheap(__LINE__)) return false;
    

    return addr;
    

}


/*
 * free
 */
void free(void* ptr)
{
    dbg_printf("\nFREE CALL AT ADDRESS %lx\n", (uint64_t)ptr - (uint64_t)mem_heap_lo());
    size_t size = GET_SIZE(HEADER(ptr));

    // Put a header and footer at the given address containing a pack of size and allocation boolean
    PUT(HEADER(ptr), PACK(size, 0)); 
    PUT(FOOTER(ptr), PACK(size, 0));

    // Run new free block address through coalesce function to check if coalecsing is necessary
    char *addr = coalesce(ptr);

    // Put Free list block in heap
    NEW_FREELIST(addr);

    if (!mm_checkheap(__LINE__)) exit(0);
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
        size_t old_size = GET_SIZE(HEADER(oldptr));
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

// Goes through the heap and outputs each header pack and address
void print_heap() {
    dbg_printf("\n\n     --- MM CHECK HEAP: HEADERS AND FOOTERS ---\n");
    dbg_printf("Low: %lx%8cHigh: %lx\n", (uint64_t)mem_heap_lo() - (uint64_t)mem_heap_lo(), ' ', (uint64_t)mem_heap_hi() - (uint64_t)mem_heap_lo());

    char *addr = heap_start;

    int count = 1;

    // Iterate through each header address of the heap
    while(GET_SIZE(HEADER(addr)) > 0){
        
        dbg_printf("---------------------------------------------------\n");
        dbg_printf("%d%11cSize|       Allocated|         Address|\n", count, ' ');        
        
        // Print header and footer
        dbg_printf("Head%12lx|%16lx|%16lx|\n", GET_SIZE(HEADER(addr)), GET_ALLOC(HEADER(addr)), (uint64_t)addr - (uint64_t)mem_heap_lo());
        dbg_printf("Foot%12lx|%16lx|%16lx|\n", GET_SIZE(FOOTER(addr)), GET_ALLOC(FOOTER(addr)), (uint64_t)addr - (uint64_t)mem_heap_lo());  
  
        count += 1;
        addr = NEXT_ADDR(addr);
    }
    dbg_printf("---------------------------------------------------\n");

    return;
}

// Goes through the free list and outputs each entry
void print_freelist() {
    dbg_printf("\n\n         --- MM CHECK HEAP: FREE LIST ---\n");
    dbg_printf("Low: %lx%8cHigh: %lx\n", (uint64_t)free_list_lo - (uint64_t)mem_heap_lo(), ' ', (uint64_t)free_list_hi - (uint64_t)mem_heap_lo());

    char *addr = free_list_lo;

    int count = 1;

    // Iterate through each free list entry of the heap
    do {
        dbg_printf("---------------------------------------------------\n");

        // Print current address, previous free address, and next free address
        dbg_printf("%d%11cPrev|            Next|         Address|\n", count, ' ');
        dbg_printf("%16lx|%16lx|%16lx|\n", (uint64_t)GET_PREV_FREE(addr) - (uint64_t)mem_heap_lo(), (uint64_t)GET_NEXT_FREE(addr) - (uint64_t)mem_heap_lo(), (uint64_t)addr - (uint64_t)mem_heap_lo());        

        count += 1;
    } while((addr = GET_NEXT_FREE(addr)) != free_list_lo);
    dbg_printf("---------------------------------------------------\n");

    return;

}

/*
 * mm_checkheap
 */
bool mm_checkheap(int lineno) {
    #ifdef DEBUG

        char *addr = heap_start;
        int count = 0;

        // Heap conditions, if any are true, print heap and corresponding error
        while(GET_SIZE(HEADER(addr)) > 0){
            if (!aligned(addr))  {
                dbg_printf("\nERROR AT LINE %d: ", lineno);
                dbg_printf("Address %lx is not aligned!\n", (uint64_t)addr - (uint64_t)mem_heap_lo());
                print_heap();
                print_freelist();
                return false;
            }
            else if ((addr == free_list_lo || addr == free_list_hi) && GET_ALLOC(addr) == 1)  {
                dbg_printf("\nERROR AT LINE %d: ", lineno);
                dbg_printf("Address %lx's allocated bit is one but is part of the free list as well\n", (uint64_t)addr - (uint64_t)mem_heap_lo());
                print_heap();
                print_freelist();
                return false;
            }
            else if ((GET_SIZE(HEADER(addr)) != GET_SIZE(FOOTER(addr))) || (GET_ALLOC(HEADER(addr)) != GET_ALLOC(FOOTER(addr))))  {
                dbg_printf("\nERROR AT LINE %d: ", lineno);
                dbg_printf("Header and footer don't match at address %lx\n", (uint64_t)addr - (uint64_t)mem_heap_lo());
                print_heap();
                print_freelist();
                return false;
            }
            else if (free_list_lo == heap_start && GET_ALLOC(HEADER(addr)) == 0)  {
                dbg_printf("\nERROR AT LINE %d: ", lineno);
                dbg_printf("Free list doesn't exist but there is a free block at address %lx\n", (uint64_t)addr - (uint64_t)mem_heap_lo());
                print_heap();
                print_freelist();
                return false;
            }
            /*else if (WRITE CONDITION HERE)  {
                dbg_printf("\nERROR AT LINE %d: ", lineno);
                dbg_printf("ERROR MESSAGE HERE!\n");
                print_heap();
                print_freelist();
                return false;
            }*/
            if (GET_ALLOC(HEADER(addr)) == 0) count += 1;
            addr = NEXT_ADDR(addr);
        }

        addr = free_list_lo;
        int count_2 = 0;
        do {            
            if (GET_ALLOC(HEADER(addr)) == 1)  {
                dbg_printf("\nERROR AT LINE %d: ", lineno);
                dbg_printf("Address %lx is part of the free list but also allocated\n", (uint64_t)addr - (uint64_t)mem_heap_lo());
                print_heap();
                print_freelist();
                return false;
            }
            else if ((GET_ALLOC(HEADER(PREV_ADDR(addr))) == 0 && GET_SIZE(HEADER(PREV_ADDR(addr))) > 0) || (GET_ALLOC(HEADER(NEXT_ADDR(addr))) == 0 && GET_SIZE(HEADER(NEXT_ADDR(addr))) > 0))  {
                dbg_printf("\nERROR AT LINE %d: ", lineno);
                dbg_printf("Coalescing failed at address %lx\n", (uint64_t)addr - (uint64_t)mem_heap_lo());
                print_heap();
                print_freelist();
                return false;
            }
            else if (false)  {
                dbg_printf("\nERROR AT LINE %d: ", lineno);
                dbg_printf("ERROR MESSAGE HERE!\n");
                print_heap();
                print_freelist();
                return false;
            }
            /*else if (WRITE CONDITION HERE)  {
                dbg_printf("\nERROR AT LINE %d: ", lineno);
                dbg_printf("ERROR MESSAGE HERE!\n");
                print_heap();
                print_freelist();
                return false;
            }*/
            count_2 += 1;
        } while((addr = GET_NEXT_FREE(addr)) != free_list_lo);

        if (GET_PREV_FREE(addr) != free_list_hi)  {
            dbg_printf("\nERROR AT LINE %d: ", lineno);
            dbg_printf("The free list's local pointer for the free_list_hi variable points to an incorrect value\n");
            print_heap();
            print_freelist();
            return false;
        } else if (count != count_2)  {
            dbg_printf("\nERROR AT LINE %d: ", lineno);
            dbg_printf("Free list has %d entries while there are %d free blocks\n", count_2, count);
            print_heap();
            print_freelist();
            return false;
        }

    #endif

    print_heap();
    print_freelist();

    return true;
}
