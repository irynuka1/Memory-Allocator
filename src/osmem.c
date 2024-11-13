#include <sys/mman.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>
#include "osmem.h"
#include "block_meta.h"

// Constants and Macros
#define MMAP_THRESHOLD (128 * 1024)        // Threshold for mmap usage (128 KB)
#define ALIGNMENT 8                        // 8-byte alignment
#define PAGE_SIZE 4080                     // Define a page size to standardize memory allocation sizes
#define ALIGN_SIZE(size) ((size + ALIGNMENT - 1) & ~(ALIGNMENT - 1))   // Align size to 8-byte boundary
#define META_SIZE sizeof(struct block_meta)

struct block_meta *heap_start = NULL;
struct block_meta *mmap_blocks = NULL;

void initialize_heap(void) {
    if (!heap_start) {
        heap_start = sbrk(0);
        void *heap_end = sbrk(MMAP_THRESHOLD);
        if (heap_end == (void *)-1) return;

        struct block_meta *initial_block = heap_start;
        initial_block->size = MMAP_THRESHOLD - META_SIZE;
        initial_block->status = STATUS_FREE;
        initial_block->next = NULL;
        initial_block->prev = NULL;
    }
}

struct block_meta *find_free_block(size_t aligned_size) {
    struct block_meta *current = heap_start;

    while (current) {
        if (current->status == STATUS_FREE && current->size >= aligned_size)
            return current;
        current = current->next;
    }
    return NULL;
}

struct block_meta *get_last_heap_block(void) {
    struct block_meta *current = heap_start;
    while (current && current->next) {
        current = current->next;
    }
    return current;
}

void extend_heap(size_t aligned_size) {
    struct block_meta *last_block = get_last_heap_block();

    if (last_block && last_block->status == STATUS_FREE && last_block->size < aligned_size) {
        size_t additional_size = aligned_size - last_block->size;
        if (sbrk(additional_size) == (void *)-1) return;

        last_block->size += additional_size;
    } else {
        struct block_meta *new_block = sbrk(aligned_size + META_SIZE);
        if (new_block == (void *)-1) return;

        new_block->size = aligned_size;
        new_block->status = STATUS_FREE;
        new_block->prev = last_block;
        new_block->next = NULL;

        if (last_block)
            last_block->next = new_block;
        else
            heap_start = new_block;
    }
}

void merge_free_blocks(void) {
    struct block_meta *current = heap_start;

    while (current && current->next) {
        if (current->status == STATUS_FREE && current->next->status == STATUS_FREE) {
            current->size += current->next->size + META_SIZE;
            current->next = current->next->next;
            if (current->next)
                current->next->prev = current;
        } else {
            current = current->next;
        }
    }
}

void split_block_if_necessary(struct block_meta *block, size_t aligned_size) {
    if (block->size > aligned_size + META_SIZE) {
        struct block_meta *new_block = (struct block_meta *)((char *)block + aligned_size + META_SIZE);
        new_block->size = block->size - aligned_size - META_SIZE;
        new_block->status = STATUS_FREE;
        new_block->next = block->next;
        new_block->prev = block;

        block->size = aligned_size;
        block->next = new_block;

        if (new_block->next)
            new_block->next->prev = new_block;
    }
}

void configure_mmap_block(struct block_meta *block, size_t size) {
    block->size = size;
    block->status = STATUS_MAPPED;
    block->prev = NULL;
    block->next = mmap_blocks;

    if (mmap_blocks)
        mmap_blocks->prev = block;
    mmap_blocks = block;
}

void *prepare_allocated_block(struct block_meta *block, size_t size) {
    block->status = STATUS_ALLOC;
    split_block_if_necessary(block, ALIGN_SIZE(size));
    return (void *)(block + 1);
}

void *os_malloc(size_t size) {
    if (size == 0)
        return NULL;

    size_t aligned_size = ALIGN_SIZE(size);
    if (aligned_size < MMAP_THRESHOLD - META_SIZE) {
        initialize_heap();

        struct block_meta *block = find_free_block(aligned_size);
        if (block)
            return prepare_allocated_block(block, size);

        extend_heap(aligned_size);
        block = find_free_block(aligned_size);
        if (block)
            return prepare_allocated_block(block, size);
    }

    void *mapped_block = mmap(NULL, aligned_size + META_SIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (mapped_block == MAP_FAILED)
        return NULL;

    struct block_meta *new_block = (struct block_meta *)mapped_block;
    configure_mmap_block(new_block, aligned_size);
    return (void *)(new_block + 1);
}

void os_free(void *ptr) {
    if (!ptr) return;

    struct block_meta *block = (struct block_meta *)((char *)ptr - META_SIZE);
    if (block->status == STATUS_FREE) return;

    struct block_meta *last_block = get_last_heap_block();
    if (block >= heap_start && block <= last_block) {
        block->status = STATUS_FREE;
        merge_free_blocks();
    } else {
        if (block == mmap_blocks)
            mmap_blocks = block->next;

        if (block->prev)
            block->prev->next = block->next;
        if (block->next)
            block->next->prev = block->prev;

        munmap(block, block->size + META_SIZE);
    }
}

void *os_calloc(size_t num_elements, size_t element_size) {
    if (num_elements == 0 || element_size == 0)
        return NULL;

    size_t total_size = ALIGN_SIZE(num_elements * element_size);
    if ((int)total_size < PAGE_SIZE) {
        void *ptr = os_malloc(total_size);
        if (ptr)
            memset(ptr, 0, total_size);
        return ptr;
    }

    void *block = mmap(NULL, total_size + META_SIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (block == MAP_FAILED)
        return NULL;

    struct block_meta *new_block = (struct block_meta *)block;
    configure_mmap_block(new_block, total_size);
    memset(block + META_SIZE, 0, total_size);
    return (void *)(new_block + 1);
}

void *os_realloc(void *ptr, size_t size) {
    return NULL;
}
