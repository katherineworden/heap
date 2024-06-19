/* Katherine Worden | CS107 | Assignment 6 CHANGE
 * This program implements an explicit heap allocator using a doubly linked list.
 * The list links freed blocks, where the first 16 bytes of each free payload each hold two (8 byte) 
 * pointers to the next and previous free blocks in the list. This allows for easy traversal to
 * allocate, reallocate, free, and coalesce memory with more efficiency.
 */ 


#include "./allocator.h"
#include <stdio.h>
#include <assert.h>
#include "./debug_break.h"
#include <string.h>

#define BYTES_PER_LINE 32
#define MINIMUM_BLOCK_SIZE 16
#define MINIMUM_PAYLOAD 8
#define ZERO_LAST 0xFFFFFFFFFFFFFFFE

typedef size_t header_t;

size_t segment_size;
void *segment_start;
void *segment_end;
size_t nused;

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
    if (heap_size < MINIMUM_BLOCK_SIZE) {  // Not enough space for header and payload
        return false;
    }
    segment_start = heap_start;
    segment_size = heap_size;
    segment_end = (char *)segment_start + segment_size;
    set_header(segment_start, segment_size - ALIGNMENT, 0);
    nused += ALIGNMENT; 
    return true;
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
        

/* Given the needed payload size, iterates over the implicit list of all blocks
 * until finding a free block with an appropriately sized payload (first fit), and returns
 * a pointer to the header of the block. 
 */
header_t *find_first(size_t needed) {
    header_t *header = segment_start; 
    while (header) {
        if ((needed <= get_payload_size(header)) & (is_free(header))) {
            return header;
        }
        header = next_header(header);
    }
    return NULL;  // could not find a free payload with the right size
}


/* Given a pointer to the payload that needs to be split, the needed bytes in that payload,
 * and the bytes remaining in the block, splits the block into another header
 * to reduce wasted unused  memory space.
 */
void split_block(void *payload, size_t needed, size_t remaining) { 
    nused += ALIGNMENT;
    header_t *new_header = (header_t *)((char *)payload + needed); 
    set_header(new_header, remaining - ALIGNMENT, 0);   
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

// Given the remaining space in the block, returns true if there's enough space for another header and payload
bool big_enough(size_t remaining) {
    if (remaining < MINIMUM_BLOCK_SIZE) {
        return true;
    }
    return false;
}


/* Simulates the "malloc" function for our implicit heap allocator. The general procedure is as follows:
 * for a given needed size, iterate over the whole heap from the start 
 * until we find the first free block with a sufficiently sized payload.
 * Return a pointer to the payload of the found free block for the client to write into. 
 */
void *mymalloc(size_t requested_size) {
    size_t needed = roundup(requested_size, ALIGNMENT);
    if (!validate_request(requested_size, needed)) {
        return NULL;
    }

    header_t *header = find_first(needed); 
    if (header == NULL) {
        return NULL;
    } 
    size_t payloadsz = get_payload_size(header); 
    void *payload = header2payload(header);
    
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


/* Given a pointer from a client to the payload of the memory they'd like to free, preforms the "free"
 * operation by freeing up the header. 
 */
void myfree(void *ptr) {
    if (ptr == NULL) {
        return; 
    }
    header_t *header = payload2header(ptr);
    size_t payloadsz = get_payload_size(header);
    set_header(header, payloadsz, 0); 
    nused -= payloadsz;
}

/* Given a pointer to the payload the client wants to reallocate, and the new size they're allocating to,
 * reallocates and returns a pointer to where the data resides after realloating. 
 */
void *myrealloc(void *old_ptr, size_t new_size) {
    if (old_ptr == NULL) {
        return mymalloc(new_size);
    }
    void *new_ptr = mymalloc(new_size);
    if (new_ptr == NULL) {
        return NULL;
    }
    size_t old_size = get_payload_size(payload2header(old_ptr));
    size_t bytes2copy = (old_size < new_size) ? old_size : new_size;      // copy the minimum of old and new size
    memcpy(new_ptr, old_ptr, bytes2copy); 
    myfree(old_ptr);
    return new_ptr;
}

/* Verifies that the current heap matches our expectations
 * for what requirements a functioning  heap should meet.  
 * Returns true if all is ok, or false otherwise.
 * This function is called periodically by the test
 * harness to check the state of the heap allocator.
 */
bool validate_heap() {
    header_t *header = segment_start;
    while (header) {
        size_t this_size = get_payload_size(header);
        if (this_size % MINIMUM_PAYLOAD != 0) {
            printf("Yikes, that is not an acceptable block payload size.\n");
            breakpoint();
            return false;
        }
        if ((char *)header > (char *)segment_end) {
            printf("Uh...you have exceeded the heap\n");
            breakpoint();
            return false;  
        }
        header = next_header(header);  
    }
    if (nused > segment_size) {
        printf("Used too much heap\n");
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
        void *payload = header2payload(header);
        if (is_free(header)) {
            status_str = 'F';
        }
        void *payload_end = (char *)payload + this_size;
        blocknum++;
        printf("%d H %p, %c (8 + %ld), S: %p E: %p \n", blocknum, header, status_str, this_size, payload, payload_end);
        header = next_header(header);       
    }
}
