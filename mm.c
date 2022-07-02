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
 //#define DEBUG

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
#ifdef DEBUG
static char *heap_list = 0; //Points to the first block in the heap
static char *listp = 0; //Points to the first free blk in the heap
static char *listp1 = 0;
static char *listp2 = 0;
static char *listp3 = 0;
static char *listp4 = 0;
static char *listp5 = 0;
static char *listp6 = 0;
static char *listp7 = 0;
static char *listp8 = 0;
static char *listp9 = 0;
static char *listp10 = 0;
static char *listp11 = 0;
static char *listp12 = 0;
static char *listp13 = 0;
static char **curr_free = &listp;
#endif
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
char *HEADER(void *addr) 
{
    return (char*)(addr) - HEAD_SIZE;
}

// return address to the footer
char *FOOTER(char *addr) 
{
    return ((char *)(addr) + GET_SIZE(HEADER(addr)) - DHEAD_SIZE); 
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

    for (addr = heap_start; GET_SIZE(HEADER(addr)) > 0; addr = NEXT_ADDR(addr)) 
    {
        if (!GET_ALLOC(HEADER(addr)) && (size <= GET_SIZE(HEADER(addr))))  
        {
            return addr;
        }
    }
    return NULL;
}

// Places a header and footer into the heap at a given address and deals with splitting if necessary, specifically used by malloc
void place(void *addr, size_t new_size)
{
    size_t old_size = GET_SIZE(HEADER(addr));

    //If splitting is necessary
    if ((old_size - new_size) >= (2*DHEAD_SIZE)) 
    {
        PUT(HEADER(addr), PACK(new_size, 1));
        PUT(FOOTER(addr), PACK(new_size, 1));
        addr = NEXT_ADDR(addr);
        PUT(HEADER(addr), PACK(old_size-new_size, 0));
        PUT(FOOTER(addr), PACK(old_size-new_size, 0));
    }
    //If splitting is not necessary
    else
    {
        PUT(HEADER(addr), PACK(old_size, 1));
        PUT(FOOTER(addr), PACK(old_size, 1));
    }
}

//Coalesces free blocks at given address
void *coalesce(void *addr)
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
        size += GET_SIZE(HEADER(NEXT_ADDR(addr)));
        PUT(HEADER(addr), PACK(size, 0));
        PUT(FOOTER(addr), PACK(size,0));
    }

    // CASE 3: Coalesce the previous block
    else if (!prev && next) { 
        size += GET_SIZE(HEADER(PREV_ADDR(addr)));
        PUT(FOOTER(addr), PACK(size, 0));
        PUT(HEADER(PREV_ADDR(addr)), PACK(size, 0));
        addr = PREV_ADDR(addr);
    }
 
    // CASE 4: Coalesce both blocks
    else { 
        size += GET_SIZE(HEADER(PREV_ADDR(addr))) +
        GET_SIZE(FOOTER(NEXT_ADDR(addr)));
        PUT(HEADER(PREV_ADDR(addr)), PACK(size, 0));
        PUT(FOOTER(NEXT_ADDR(addr)), PACK(size, 0));
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
    PUT(HEADER(addr), PACK(size, 0));  
    PUT(FOOTER(addr), PACK(size, 0)); 
    PUT(HEADER(NEXT_ADDR(addr)), PACK(0, 1)); 

    mm_checkheap(__LINE__);

    // Coalesce if the previous block was free 
    return coalesce(addr);
 }

/*
 * Initialize: returns false on error, true on success.
 */
bool mm_init(void)
{
    #ifdef DEBUG
    heap_list += (2*HEAD_SIZE);
    listp = heap_list;
    listp1 = heap_list;
    listp2 = heap_list;
    listp3 = heap_list;
    listp4 = heap_list;
    listp5 = heap_list;
    listp6 = heap_list;
    listp7 = heap_list;
    listp8 = heap_list;
    listp9 = heap_list;
    listp10 = heap_list;
    listp11 = heap_list;
    listp12 = heap_list;
    listp13 = heap_list;
    #endif
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

    if (!mm_checkheap(__LINE__)) return false;
    else return addr;
    

}


/*
 * free
 */
void free(void* ptr)
{
    size_t size = GET_SIZE(HEADER(ptr));

    // Put a header and footer at the given address containing a pack of size and allocation boolean
    PUT(HEADER(ptr), PACK(size, 0)); 
    PUT(FOOTER(ptr), PACK(size, 0)); 

    // Run new free block address through coalesce function to check if coalecsing is necessary
    coalesce(ptr);

    mm_checkheap(__LINE__);
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

//Function to Go through the heap and output each header pack and address
void print_heap() {
    dbg_printf("---------------- MM CHECK HEAP ----------------\n");

    void *addr = heap_start;

    int count = 1;

    // Iterate through each header address of the heap
    while(GET_SIZE(HEADER(addr)) > 0){
        
        // Print headers
        dbg_printf("---------------------------------------------------\n");
        dbg_printf("Head %6d: Size|       Free|    Address|\n", count);
        dbg_printf("%16lx|%16lx|%16lx|\n", GET_SIZE(HEADER(addr)), GET_ALLOC(HEADER(addr)), GET(HEADER(addr)));        
        
        // Print footers
        dbg_printf("Foot %6d: Size|       Free|    Address|\n", count);
        dbg_printf("%16lx|%16lx|%16lx|\n", GET_SIZE(FOOTER(addr)), GET_ALLOC(FOOTER(addr)), GET(FOOTER(addr)));  

        count += 1;
        addr = NEXT_ADDR(addr);
    }

    return;
}

/*
 * mm_checkheap
 */
bool mm_checkheap(int lineno) {
    #ifdef DEBUG

        void *addr = heap_start;

        // Heap conditions, if any are true, print heap and corresponding error
        while(GET_SIZE(HEADER(addr)) > 0/* && GET_SIZE(HEADER(addr)) < 10*/){
            if (!aligned(addr))  {
                print_heap();
                dbg_printf("\nERROR AT LINE %d: ", lineno);
                dbg_printf("Address %lx is not aligned!\n", (uint64_t)addr);
                return false;
            }
            /*else if (WRITE CONDITION HEREfalse)  {
                print_heap();
                dbg_printf("\nERROR AT LINE %d: ", lineno);
                dbg_printf("ERROR MESSAGE HERE!\n");
                return false;
            }*/

            //else if
        if(lineno >= 0 && lineno <15){
            char **curr; 
            dbg_printf("CURR FREELIST: %p\n", curr_free); 
            if(lineno == 0)
            {
                curr = &listp;
                dbg_printf("FREE 0: %p\n", &listp); 
            }
            else if(lineno == 1)
            {
                curr = &listp1;
                dbg_printf("FREE 1: %p\n", &listp1); 
            }	
            else if(lineno == 2)
            {
                curr = &listp2;
                dbg_printf("FREE 2: %p\n", &listp2); 
            }
            else if(lineno == 3)
            {
                curr = &listp3;
                dbg_printf("FREE 3: %p\n", &listp3); 
            }
            else if(lineno == 4)
            {	
                curr = &listp4;
                dbg_printf("FREE 4: %p\n", &listp4); 
            }
            else if(lineno == 5)
            {
                curr = &listp5;
                dbg_printf("FREE 5: %p\n", &listp5); 
            }
            else if(lineno == 6)
            {
                curr = &listp6;
                dbg_printf("FREE 6: %p\n", &listp6); 
            }
            else if(lineno == 7)
            {
                curr = &listp7;
                dbg_printf("FREE 7: %p\n", &listp7); 
            }
            else if(lineno == 8)
            {
                curr = &listp8;
                dbg_printf("FREE 8: %p\n", &listp8); 
            }
            else if(lineno == 9)
            {
                curr = &listp9;
                dbg_printf("FREE 9: %p\n", &listp9); 
            }
            else if(lineno == 10)
            {
                curr = &listp10;
                dbg_printf("FREE 10: %p\n", &listp10); 
            }
            else if(lineno == 11)
            {
                curr = &listp11;
                dbg_printf("FREE 11: %p\n", &listp11); 
            }
            else if(lineno == 12)
            {
                curr = &listp12;
                dbg_printf("FREE 12: %p\n", &listp12); 
            }
            else if(lineno == 13)
            {
                curr = &listp13;
                dbg_printf("FREE 13: %p\n", &listp13); 
            }
            else
            {
                dbg_printf("AN ERROR HAS OCCURED WITH THE FREELIST\n");
                return false; 
            }
            dbg_printf("\n"); 
            
            if(curr == &heap_list)
            {
                dbg_printf("heap_list = %p\n", heap_list); 
                dbg_printf("curr = %p\n\n", *curr); 
                dbg_printf("FREE EMPTY!\n\n");
                return false; 
        }
            //...
        }
        addr = NEXT_ADDR(addr);
    }
    print_heap();    
    #endif
    return true;
}
