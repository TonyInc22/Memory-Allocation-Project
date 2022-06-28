/*
 * mm.c
 *
 * Name: [FILL IN]
 *
 * NOTE TO STUDENTS: Replace this header comment with your own header
 * comment that gives a high level description of your solution.
 * Also, read malloclab.pdf carefully and in its entirety before beginning.
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
#define BUFFER 8
#define BLOCK_SIZE 8

// Struct for linked list header and its size definition
struct header_s {
    size_t size;
    bool prev_free;
    bool free;
};
typedef struct header_s header;

struct free_list_s {
    void * next;
    void * prev;
};
typedef struct free_list_s free_list;

free_list *first_free;
free_list *last_free;

/* rounds up to the nearest multiple of ALIGNMENT */
static size_t align(size_t x)
{
    return ALIGNMENT * ((x+ALIGNMENT-1)/ALIGNMENT);
}

uint64_t compress_header(header uncompressed){
    uint64_t compressed = (uint64_t)uncompressed.free | ((uint64_t)uncompressed.prev_free << 4) | ((uint64_t)uncompressed.size << 8);
    return compressed;
}

header decompress_header(uint64_t compressed){
    header uncompressed = {(uint64_t)compressed >> 8, (uint64_t)compressed >> 4, (uint64_t)0x1 & compressed};
    return uncompressed;
}

void write_footer(void * addr, uint64_t data) {
    mem_write(addr, data, BLOCK_SIZE);
    return;
}

/*
 * Initialize: returns false on error, true on success.
 */
bool mm_init(void)
{
    if (!mem_sbrk(BLOCK_SIZE)) return false;
    uint64_t buf = NULL;
    void * addr = mem_heap_lo();
    mem_write(addr, buf, BLOCK_SIZE);
    first_free = buf;
    return true;
}

/*
 * malloc
 */
void* malloc(size_t size)
{
    //Check if size requested is invalid
    if (size <= 0) {
        printf("\nError in malloc: given size is less than or equal to 0");
        return NULL;
    }

    //align size
    size_t new_size = align(size);

    //generate free list
    free_list *node = first_free;

    /*CASE 1: Free list points to nothing
        - use mem_sbrk to create space
        - create, compress, and write header into heap
    */
    if (node == NULL) {
        new_size = align(size + BLOCK_SIZE);
        void* addr = mem_sbrk(new_size);
        header head = {new_size,false};
        
        uint64_t compressed_head = compress_header(head);
        mem_write(addr, compressed_head,BLOCK_SIZE);
        
        return addr + BLOCK_SIZE;
    }

    /*CASE 2:Try to find first fit in free list
        - Traverse free list
        - Check if any free node has enough size
        - If so, 
            - rewrite header
            - remove node from free list with split if needed
            - update the prev and next nodes of the free list
    */

    //Traverse List
    while(node != NULL) {
        // printf("    CASE 2: First fit\n");   
        void *head_addr = node - BLOCK_SIZE;
        uint64_t old_compressed_head = mem_read(head_addr, BLOCK_SIZE);
        header head = decompress_header(old_compressed_head);

        //Found a fit
        if (head.size <= new_size) {
            head.free = false;
            head.prev_free = false;
            uint64_t new_compressed_head;

            //Splitting is necessary
            if(head.size != new_size) {

                //Create new header & footer and write it in the heap
                size_t new_head_size = head.size - new_size - BLOCK_SIZE;
                header new_head = {new_head_size, true}; 
                uint64_t compressed_new_head = compress_header(new_head);
                void * new_head_addr = head_addr + BLOCK_SIZE + new_size + BLOCK_SIZE;
                mem_write(new_head_addr,compressed_new_head, BLOCK_SIZE);
                write_footer(new)

                //prepare rewrite of header at fitted free spot
                head.size = new_size;
                new_compressed_head = compress_header(head);

                //Create new free list node address and update free list
                node = head_addr + BLOCK_SIZE + new_size + BLOCK_SIZE;
                if (node->prev == NULL) first_free = node;
                else {
                    free_list *prev = node->prev;
                    prev->next = node;
                    mem_write((void *)prev, (uint64_t)prev, BLOCK_SIZE);
                    }
                if  (node->next != NULL) {
                    free_list *next = node->next;
                    next->prev = node;
                    mem_write((void *)next, (uint64_t)next, BLOCK_SIZE);
                }   
            }
            //Splitting is not necessary
            else {
                new_compressed_head = compress_header(head);

                free_list *prev = node->prev;
                free_list *next = node->next;
                prev->next = next;
                next->prev = prev;
                mem_write((void *)prev, (uint64_t)prev, BLOCK_SIZE);
                mem_write((void *)next, (uint64_t)next, BLOCK_SIZE);
            }
            mem_write(head_addr, new_compressed_head, BLOCK_SIZE);
            return head_addr + BLOCK_SIZE;
        }
        node = node->next;
    }

    /*CASE 3: No available space, need to request more space
        - use mem_sbrk to create space
        - create, compress, and write header into heap
    */
    // printf("    CASE 3: ASK FOR MORE SPACE\n");
    void *addr = mem_sbrk(new_size + BLOCK_SIZE);
    header new_head = {new_size,false};

    // check if last block in the heap is free then set prev_free in header
    uint64_t last_free_head_compressed = mem_read(last_free - BLOCK_SIZE, BLOCK_SIZE);
    header last_free_head = decompress_header(last_free_head_compressed);
    if (last_free_head.size + last_free >= mem_heap_hi()) new_head.prev_free = true;

    uint64_t compressed_new_head = compress_header(new_head);
    mem_write(addr, compressed_new_head, BLOCK_SIZE);

    return addr + BLOCK_SIZE;
}

/*
 * free
 */
void free(void* ptr)
{

    
    
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

/*
 * Returns whether the pointer is in the heap.
 * May be useful for debugging.
 */
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
    
//#ifdef DEBUG
    void * current_addr = mem_heap_lo() + BLOCK_SIZE;    

    printf("\nALL LINES IN THE HEAP\n");
    printf("--------------------------\n\n");

    int count = 1; 
    while(current_addr < mem_heap_hi()){        

        uint64_t old_compressed_head = mem_read(current_addr, BLOCK_SIZE);
        header head = decompress_header(old_compressed_head);
        printf("------------------------------------------------------------\n");
        printf("Head %d:                                address: %lx\n", count, (uint64_t) (current_addr - mem_heap_lo()));
        printf("Size (dec): %lu (hex): %lx\n", (uint64_t)head.size, (uint64_t)head.size);
        printf("Free: %lu\n", (uint64_t)head.free);
        printf("------------------------------------------------------------\n");
 
        current_addr = (void *) (current_addr + head.size);
        count++;
    }
    

//#endif /* DEBUG */
    return true;
}
