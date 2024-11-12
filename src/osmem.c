#include <sys/mman.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>
#include <stdint.h>
#include "osmem.h"
#include "block_meta.h"

#define MMAP_THRESHOLD (128 * 1024)
#define MAP_ANONYMOUS 0x20
#define PAGE_SIZE 4080
#define ALIGN_BYTE(size) ((size + 7) & (~7))
#define BLOCK_SIZE sizeof(struct block_meta)

struct block_meta *global_base;
struct block_meta *large_alloc;

void init_heap(void)
{
    if (!global_base) {
        global_base = sbrk(0);
        void *heap_end = sbrk(MMAP_THRESHOLD);

        if (heap_end == (void *)-1)
            return;

        struct block_meta *first_block = global_base;
        first_block->size = MMAP_THRESHOLD - BLOCK_SIZE;
        first_block->status = STATUS_FREE;
        first_block->next = NULL;
        first_block->prev = NULL;
    }
}

struct block_meta *get_free_block(size_t size)
{
    struct block_meta *current = global_base;

    while (current) {
        if (current->status == STATUS_FREE && current->size >= size)
            return current;

        current = current->next;
    }
    return NULL;
}

struct block_meta *get_last_block(void)
{
    struct block_meta *current = global_base;

    while (current && current->next)
        current = current->next;

    return current;
}

void expand_heap(size_t size) {
    struct block_meta *last = get_last_block();
    
    if (last && last->status == STATUS_FREE && last->size < size) {
        size_t additional_size = size - last->size;
        if (sbrk(additional_size) == (void *)-1)
            return;

        last->size += additional_size;
    } else {
        struct block_meta *new_block = sbrk(size + BLOCK_SIZE);
        if (new_block == (void *)-1)
            return;

        new_block->size = size;
        new_block->status = STATUS_FREE;
        new_block->prev = last;
        new_block->next = NULL;

        if (last)
            last->next = new_block;
        else
            global_base = new_block;
    }
}

void coalesce_blocks(void) {

    struct block_meta *current = global_base;

    while (current && current->next) {
        if (current->status == STATUS_FREE && current->next->status == STATUS_FREE) {
            current->size += current->next->size + BLOCK_SIZE;
            current->next = current->next->next;
            if (current->next)
                current->next->prev = current;
        } else {
            current = current->next;
        }
    }
}

void split_block(struct block_meta *block, size_t size) {
    if (block->size > size + BLOCK_SIZE) {
        struct block_meta *new_block = (struct block_meta *)((char *)block + size + BLOCK_SIZE);
        new_block->size = block->size - size - BLOCK_SIZE;
        new_block->status = STATUS_FREE;
        new_block->next = block->next;
        new_block->prev = block;

        block->size = size;
        block->next = new_block;

        if (new_block->next)
            new_block->next->prev = new_block;
    }
}

void *os_malloc(size_t size) {
    if (size == 0)
        return NULL;

    size_t aligned_size = ALIGN_BYTE(size);
    if (aligned_size < MMAP_THRESHOLD - BLOCK_SIZE) {
        init_heap();

        struct block_meta *block = get_free_block(aligned_size);
        if (block) {
            block->status = STATUS_ALLOC;
            split_block(block, aligned_size);
            return (void *)(block + 1);
        }

        expand_heap(aligned_size);
        block = get_free_block(aligned_size);
        if (block) {
            block->status = STATUS_ALLOC;
            split_block(block, aligned_size);
            return (void *)(block + 1);
        }
    }

    void *mapped_block = mmap(NULL, aligned_size + BLOCK_SIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (mapped_block == MAP_FAILED)
        return NULL;

    struct block_meta *new_block = (struct block_meta *)mapped_block;
    new_block->size = aligned_size;
    new_block->status = STATUS_MAPPED;
    new_block->prev = NULL;
    new_block->next = large_alloc;

    if (large_alloc)
        large_alloc->prev = new_block;
    large_alloc = new_block;

    return (void *)(new_block + 1);
}

// free a block
void os_free(void *ptr) {
    if (!ptr)
        return;

    struct block_meta *block = (struct block_meta *)((char *)ptr - BLOCK_SIZE);

    if (block->status == STATUS_FREE)
        return;

    struct block_meta *last_block = get_last_block();
    if (block >= global_base && block <= last_block) {
        block->status = STATUS_FREE;
        coalesce_blocks();
        return;
    }

    if (block == large_alloc)
        large_alloc = block->next;

    if (block->prev)
        block->prev->next = block->next;

    if (block->next)
        block->next->prev = block->prev;

    block->status = STATUS_FREE;
    munmap(block, block->size + BLOCK_SIZE);
}

void *os_calloc(size_t nmemb, size_t size)
{
	// /* TODO: Implement os_calloc */
	// size_t cc = size * nmemb;
	// size_t new_size = ALIGN_BYTE(cc);

	// if (nmemb == 0 || size == 0)
	// 	return NULL;

	// if ((int)new_size < PAGE_SIZE) {
	// 	void *ptr = os_malloc(new_size);

	// 	if (ptr)
	// 		memset(ptr, 0, new_size);
	// 	return ptr;
	// }
	// void *block = mmap(NULL, new_size + BLOCK_SIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

	// if (block == MAP_FAILED)
	// 	return NULL;

	// struct block_meta *new_block = block;

	// new_block->size = new_size;
	// new_block->status = STATUS_MAPPED;
	// new_block->prev = NULL;
	// new_block->next = large_alloc;
	// if (large_alloc)
	// 	large_alloc->prev = new_block;
	// large_alloc = new_block;
	// memset(block + 32, 0, new_size);
	// return (void *)(new_block + 1);
}

void *os_realloc(void *ptr, size_t size)
{
	return NULL;
}
