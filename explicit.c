/* Katherine Worden | CS107 | Assignment 6
 * This program implements an explicit heap allocator using a doubly linked list.
 * The list links freed blocks, where the first 16 bytes of each free payload each hold two (8 byte) 
 * pointers to the next and previous free blocks in the list. This allows for easy traversal to
 * allocate, reallocate, free, and coalesce memory with more efficiency.
 */ 


#include "./allocator.h"
#include "./debug_break.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>


#define BYTES_PER_LINE 32
#define MINIMUM_BLOCK_SIZE 24
#define MINIMUM_PAYLOAD_SIZE 16
#define ZERO_LAST 0xFFFFFFFFFFFFFFFE

typedef size_t header_t;

size_t segment_size;
void *segment_start;
void *segment_end;
struct node* fl_front;
size_t nused;
struct node {
    header_t* prev; 
    header_t* next;
};

// Given a pointer to a block header, returns 1 or 0 if the block is free
bool is_free(header_t *header) {
    return !(*header & 1);
}

// Given a pointer to aheader, the payload size, and the desired status of the block, sets the header information
void set_header(header_t *header, size_t size, int status) {  
    *header = (size |= status);
}

// Given a pointer to a header, returns the payload size by zeroing out the last bit
size_t get_payload_size(header_t *header) {
    return ((*header) & ZERO_LAST);  
}

// Given a pointer to a header, returns a pointer to the start of the block payload
void *header2payload(header_t *header) {
    return ((char *)header + ALIGNMENT); 
}

// Given a pointer to a block's payload, returns a pointer to the header
header_t *payload2header(void *payload) {
    return (header_t *)((char *)payload - ALIGNMENT);
}

// Given a pointer to a header, uses the determined block size to find and return a pointer to the next header
header_t *next_header(header_t *header) { 
    size_t payload_size = get_payload_size(header);
    void *payload = header2payload(header);
    header_t *n_header = (header_t *)((char *)payload + payload_size);
    if (n_header == segment_end) {
        return NULL; 
    }
    return n_header;
}

/* Function: roundup
 * -----------------
 * This function rounds up the given number to the given multiple, which
 * must be a power of 2, and returns the result. This code was directly copied from 
 * the given code in Bump.c.
 */
size_t roundup(size_t sz, size_t mult) {
    return (sz + mult - 1) & ~(mult - 1);
}

/* Given a pointer to a free block and pointers to two other free blocks, 
 * sets the prev and next pointers in the node, respectively. 
 */
void set_nodes(struct node* payload, header_t* new_prev, header_t* new_next) {
    payload->prev = new_prev;
    payload->next = new_next;
}


// Given a pointer to a header, returns a pointer to the header of the previous free block, by traversing the list
header_t* prev_free(header_t* header) { 
    struct node *n = header2payload(header);
    return n->prev;
}

// Given a pointer to a header, returns a pointer to the header of the next free block, by traversing the list
header_t* next_free(header_t* header) {
    struct node *n = header2payload(header);
    return n->next;
}

/* Given a pointer to the payload of a free block we're adding, 
 * adds the given block to the front of the explicit list by rewiring surrounding pointers, following
 * first-in first-out ordering. 
 */
void add_free_block(struct node *new_free_payload) {
    if (!fl_front) {   // The list is currently empty (No free blocks)
        set_nodes(new_free_payload, NULL, NULL);
    } else {
        fl_front->prev = payload2header(new_free_payload); 
        set_nodes(new_free_payload, NULL, payload2header(fl_front));
    }
    fl_front = new_free_payload;  
}

/* Given a pointer to the payload of a block we're removing, 
 * detaches the given block from its place in the explicit list by rewiring surrounding pointers.
 */
void detach_free_block(struct node *free_payload) {
    if (fl_front == free_payload) {   // Edge case 1: I'm removing from the front of the list
        if (!free_payload->next) {   // Edge case 2: I'm removing the only block in the list
            fl_front = NULL;
            return;
        } else {
            fl_front = header2payload(free_payload->next);
            fl_front->prev = NULL; 
        }
    } else { 
        header_t* last_free = free_payload->prev; 
        struct node *last_node = header2payload(last_free);
        last_node->next = free_payload->next;
    }
    if (free_payload->next) {   // Edge case 3: I'm removing from the BACK of the list
        struct node* next_node = header2payload(free_payload->next);
        next_node->prev = free_payload->prev;
    }
    set_nodes(free_payload, NULL, NULL);
}

/* Given a pointer to the header of a block, returns the next free block by address order in the heap,
 * as opposed to the next free block in the explicit list. Done by traversing over every block. 
 */
header_t* next_free_block(header_t* free_block_ptr) {
    free_block_ptr = next_header(free_block_ptr);
    while (free_block_ptr) {
        if (is_free(free_block_ptr)) {
            return free_block_ptr;
        }
        free_block_ptr = next_header(free_block_ptr);
    }
    return NULL;
}

/* Given the needed payload size, iterates over the explicit list of free blocks
 * until finding a block with an appropriately sized payload (first fit), and returns
 * a pointer to the header of the block. 
 */
header_t *find_first(size_t needed) {
    header_t *header = payload2header(fl_front); 
    while (header != NULL) { 
        if (needed <= get_payload_size(header)) {
            return header;
        }
        header = next_free(header);
    }
    return NULL;  // We could not find an adequately sized payload
}

/* Myinit initalizes the allocator by setting global variables and verifying
 * the client has provided a heap size at least as large as the minimum block size. 
 * Myinit is called by a client before making any allocation
 * requests. The function returns true if initialization was
 * successful, or false otherwise. The myinit function can be
 * called to reset the heap to an empty state. When running
 * against a set of of test scripts, the test harness calls
 * myinit before starting each new script. 
 */
bool myinit(void *heap_start, size_t heap_size) {
    if (heap_size < MINIMUM_BLOCK_SIZE) { 
        return false;
    }
    segment_start = heap_start;
    segment_size = heap_size;
    segment_end = (char *)heap_start + heap_size;
    set_header(segment_start, segment_size - ALIGNMENT, 0);
    fl_front = header2payload(segment_start);
    set_nodes(fl_front, NULL, NULL);
    nused += ALIGNMENT; 
    return true;
}

/* Given a pointer to the payload that needs to be split, the needed bytes in that payload,
 * and the bytes remaining in the block, splits the block into another header
 * to reduce wasted unused  memory space.
 */
void split_block(void *payload, size_t needed, size_t remaining) { 
    nused += ALIGNMENT;
    header_t *new_header = (header_t *)((char *)payload + needed); 
    set_header(new_header, remaining - ALIGNMENT, 0);
    add_free_block(header2payload(new_header));    
}

// Given the requested size its rounded up counterpart (needed), returns false if any requisite malloc conditions fail
bool validate_request(size_t needed, size_t requested_size) {
    if (requested_size == 0) {
        return 0;
    }
    if (needed + nused > segment_size) {
        printf("OUT OF MEMORY; CANNOT SERVICE REQUEST\n");
        return 0;
    }
    if (needed > MAX_REQUEST_SIZE) {
        return 0; 
    }
    return true;
}

// Returns true if the given remaining space is large enough for a new header and payload 
bool big_enough(size_t remaining) {
    if (remaining < MINIMUM_BLOCK_SIZE) {
        return true;
    }
    return false;
}

/* Simulates the "malloc" function for our explicit heap allocator. The general procedure is as follows:
 * for a given needed size, iterate over the explicit list until we find the first sufficiently 
 * sized payload, then remove that block from the free list. If large enough, split the block 
 * to minimize wasted memory space. Finally, return a pointer to 
 * the payload of the found free block for the client to write into. 
 */
void *mymalloc(size_t requested_size) {
    size_t needed = roundup(requested_size, ALIGNMENT);
    needed = (needed < MINIMUM_PAYLOAD_SIZE) ? MINIMUM_PAYLOAD_SIZE : needed; 
    if (!validate_request(requested_size, needed)) {
        return NULL;
    }
    header_t *header = find_first(needed); 
    if (!header) {
        return NULL; 
    }
    struct node* payload = header2payload(header);
    detach_free_block(payload);

    size_t payloadsz = get_payload_size(header);
    size_t remaining = payloadsz - needed;
    if (big_enough(remaining)) {
        needed = payloadsz;   // Taking everything;
    } else { 
        split_block(payload, needed, remaining); 
    }

    set_header(header, needed, 1);
    nused += needed;
    return payload;
}

// Merges two free blocks into one block by removing one from the free list and updating the header of the original
void merge_blocks(header_t* new_free_block, size_t payload2merge) {
    size_t orig_payloadsz = get_payload_size(new_free_block);  
    size_t new_payloadsz = orig_payloadsz + payload2merge;
    set_header(new_free_block, new_payloadsz, 0);
}

/* Given a pointer to a free block, continuously determines if its right neighbor is free,
 * and if so, merges them into a single free block.
 */
void coalesce(header_t* new_free_block) {
    size_t payload2merge = 0;
    header_t* right_neighbor = next_header(new_free_block);
    while (right_neighbor) {
        if (!is_free(right_neighbor)) {
            break;
        }
        payload2merge += (ALIGNMENT + get_payload_size(right_neighbor));
        struct node* right_nnode = header2payload(right_neighbor);
        detach_free_block(right_nnode);
        nused -= ALIGNMENT;
        right_neighbor = next_header(right_neighbor);;
    }
    if (payload2merge > 0) {
        merge_blocks(new_free_block, payload2merge);
    }
}

/* Given a pointer from a client to the payload of the memory they'd like to free, preforms the "free"
 * operation by freeing up the header, attempting to coalesce, and adding the new block 
 * back into the explicit list. 
 */
void myfree(void *ptr) {
    if (ptr == NULL) {
        return;
    }
    header_t *header = payload2header(ptr);
    size_t payloadsz = get_payload_size(header);
    struct node* new_free_payload = ptr;
    add_free_block(new_free_payload);
    set_header(header, payloadsz, 0); 
    coalesce(header); 
    nused -= payloadsz;
}

/* Performs an in-place reallocation, which memmoves the data, the splits the block if necessary,
 * and resets the size of the previous header. 
 */
void realloc_inplace(size_t bytes2copy, void *old_ptr, size_t needed, size_t post_cs_size, header_t *old_header) {
    memmove(old_ptr, old_ptr, bytes2copy);
    size_t remaining = post_cs_size - needed;
    if (big_enough(remaining)) {
        needed = post_cs_size;  
    } else { 
        split_block(old_ptr, needed, remaining); 
    }
    set_header(old_header, needed, 1);
    nused += needed;
}

/* Given a pointer to the payload the client wants to reallocate, and the new size they're allocating to,
 * reallocates and returns a pointer to where the data resides after realloating. Begins by coalescing the
 * free blocks as much as possible, then attempts to reallocate in place if possible, otherwise
 * moves the data to a new memory location via a call to my malloc. 
 */
void *myrealloc(void *old_ptr, size_t new_size) {
    size_t needed = roundup(new_size, ALIGNMENT);
    needed = (needed < MINIMUM_PAYLOAD_SIZE) ? MINIMUM_PAYLOAD_SIZE : needed;
    
    if (old_ptr == NULL) { 
        return mymalloc(new_size); 
    }
    if (!validate_request(new_size, needed)) {
        return NULL;
    }
    header_t* old_header = payload2header(old_ptr);
    size_t old_size = get_payload_size(old_header);
    coalesce(old_header);
    size_t post_cs_size = get_payload_size(old_header); 

    if (needed <= post_cs_size) {   // Treats shrinking and growing in-place the same
        size_t bytes2copy = (old_size < new_size) ? old_size : new_size; 
        realloc_inplace(bytes2copy, old_ptr, needed, post_cs_size, old_header);
        return old_ptr;
    } else {
        void *new_ptr = mymalloc(new_size);   // There wasn't enough space, even after coalescing
        if (new_ptr == NULL) {
            return NULL;
        } 
        memcpy(new_ptr, old_ptr, new_size); 
        myfree(old_ptr); 
        nused += needed;
        return new_ptr;
    }
}

/* Verifies that the current heap matches our expectations
 * for what requirements a functioning  heap should meet.  
 * Returns true if all is ok, or false otherwise.
 * This function is called periodically by the test
 * harness to check the state of the heap allocator.
 */
bool validate_heap() {
    int num_blocks = 0;
    int free_list_size = 0;
    int num_free_blocks = 0;
    
    header_t *header = payload2header(fl_front);
    while (header != NULL) {
        if (!is_free(header)) {
            printf(" >:( What are you doing in my explicit list\n");
            breakpoint();
            return false;
        }
        free_list_size++;
        header = next_free(header);
    }
    header = segment_start;
    size_t live_count = 0;
    while (header) { 
        size_t this_size = get_payload_size(header);
        if (this_size % ALIGNMENT != 0) {
            printf("Yikes, that is not an acceptable block payload size.\n");
            breakpoint();
            return false;
        }
        if ((char *)header > (char *)segment_end) {
            printf("Uh...you have exceeded the heap\n");
            breakpoint();
            return false;
        }
        num_blocks++;
        if (is_free(header)) {
            num_free_blocks++;
        }
        live_count += ALIGNMENT + this_size;
        header = next_header(header);
    }
    if (live_count > segment_size) { 
        printf("Used too much heap: Used: %ld Size: %ld \n", live_count, segment_size);
        breakpoint();
        return false;
    }
    if (num_free_blocks < free_list_size) {
        printf("You might be listing extra blocks in your free list?\n"); 
        breakpoint();
        return false;
    } else if (num_free_blocks > free_list_size) {
        printf("You're missing someone!!!\n");
        breakpoint();
        return false;
    }
    return true;
}

/* Function: dump_heap
 * -------------------
 * This function prints out the the block contents of the heap.  It is not
 * called anywhere, but is a useful helper function to call from gdb when
 * tracing through programs.  It prints out the total range of the heap, and
 * information about each block within it.
 */
void dump_heap() {
    header_t *header = segment_start;
    int blocknum = 0;
    while (header) {
        char status_str = 'A';
        size_t this_size = get_payload_size(header);
        char str_ex[BYTES_PER_LINE * 2]; 
        if (is_free(header)) {
            status_str = 'F';
            header_t* next_f = next_free(header);
            header_t* prev_f = prev_free(header);
            sprintf(str_ex, "P: %p, N: %p", next_f, prev_f);
        } else {
            void *payload = header2payload(header);
            void *payload_end = (char *)payload + this_size;
            sprintf(str_ex, "S: %p, E: %p", payload, payload_end);
        }
        blocknum++;
        printf("%d %p, %c (8 + %ld) %s\n", blocknum, header, status_str, this_size, str_ex);
        header = next_header(header);       
    }
    printf("Free list start: %p\n", fl_front);
}
