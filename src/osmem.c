#include <sys/mman.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>
#include "osmem.h"
#include "block_meta.h"

struct block_meta *heap_start = NULL;
struct block_meta *mmap_blocks = NULL;
int is_calloc_called = 0;

void initialize_heap(void) {
    if (!heap_start) {
        heap_start = sbrk(0);
        void *heap_end = sbrk(MMAP_THRESHOLD);
        if (heap_end == (void *)-1)
            return;

        struct block_meta *initial_block = heap_start;
        initial_block->size = MMAP_THRESHOLD - META_SIZE;
        initial_block->status = STATUS_FREE;
        initial_block->next = NULL;
        initial_block->prev = NULL;
    }
}

struct block_meta *get_last_heap_block(void) {
    struct block_meta *current = heap_start;
    while (current && current->next) {
        current = current->next;
    }
    return current;
}

void *extend_heap(size_t aligned_size) {
    struct block_meta *last_block = get_last_heap_block();

    if (last_block && last_block->status == STATUS_FREE && last_block->size < aligned_size) {
        size_t additional_size = aligned_size - last_block->size;
        if (sbrk(additional_size) == (void *)-1)
            return;

        last_block->size += additional_size;
    } else {
        struct block_meta *new_block = sbrk(aligned_size + META_SIZE);
        if (new_block == (void *)-1)
            return;

        new_block->size = aligned_size;
        new_block->status = STATUS_FREE;
        new_block->prev = last_block;
        new_block->next = NULL;

        if (last_block)
            last_block->next = new_block;
        else
            heap_start = new_block;
    }

    for (last_block = heap_start; last_block; last_block = last_block->next)
        if (last_block->status == STATUS_FREE && last_block->size >= aligned_size)
            return (void *)last_block;
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

void *prepare_allocated_block(struct block_meta *block, size_t size, size_t aligned_size) {
    block->status = STATUS_ALLOC;

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

    return (void *)(block + 1);
}

void *os_malloc(size_t size) {
    if (size == 0)
        return NULL;

    size_t aligned_size = ALIGN_SIZE(size);
    size_t check_size;
    if (is_calloc_called == 1)
        check_size = PAGE_SIZE;
    else
        check_size = MMAP_THRESHOLD - META_SIZE;

    if (aligned_size < check_size) {
        initialize_heap();
        struct block_meta *block;
        for (block = heap_start; block; block = block->next)
            if (block->status == STATUS_FREE && block->size >= aligned_size)
                break;

        if (!block)
            block = extend_heap(aligned_size);

        block = prepare_allocated_block(block, size, aligned_size);
        if (is_calloc_called == 1) {
            is_calloc_called = 0;
            memset(block, 0, size);
        }
        return (void *)block;
    }

    void *mapped_block = mmap(NULL, aligned_size + META_SIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (mapped_block == MAP_FAILED)
        return NULL;

    if (is_calloc_called == 1) {
        is_calloc_called = 0;
        memset(mapped_block + META_SIZE, 0, size);
    }
    configure_mmap_block((struct block_meta *)mapped_block, aligned_size);
    return (void *)((struct block_meta *)mapped_block + 1);
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

void os_free(void *ptr) {
    if (!ptr)
        return;

    struct block_meta *block = (struct block_meta *)((char *)ptr - META_SIZE);
    if (block->status == STATUS_FREE)
        return;

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

    is_calloc_called = 1;
    size_t total_size = ALIGN_SIZE(num_elements * element_size);
    return os_malloc(total_size);
}

void *os_realloc(void *ptr, size_t size) {
    return NULL;
}
