// SPDX-License-Identifier: BSD-3-Clause

#include <errno.h>
#include <stdio.h>
#include <sys/mman.h>
#include <unistd.h>
#include <string.h>

#include "../utils/block_meta.h"
#include "../utils/osmem.h"
#include "../utils/printf.h"
#include "defines.h"

static struct block_meta *global_start;
static size_t global_treshold = MMAP_THRESHOLD;
static short int global_status = STATUS_FREE;

void asign_block(struct block_meta **block, size_t size, int status, struct block_meta *prev, struct block_meta *next)
{
	(*block)->size = size;
	(*block)->status = status;
	(*block)->prev = prev;
	(*block)->next = next;
}

void coalesce(struct block_meta **block)
{
	if ((*block)->next && (*block)->next->status == STATUS_FREE) {
		(*block)->size += (*block)->next->size + SIZE_BLOCK_META;
		(*block)->next = (*block)->next->next;

		if ((*block)->next)
			(*block)->next->prev = (*block);
	}

	if ((*block)->prev && (*block)->prev->status == STATUS_FREE) {
		(*block)->prev->size += (*block)->size + SIZE_BLOCK_META;
		(*block)->prev->next = (*block)->next;

		if ((*block)->next)
			(*block)->next->prev = (*block)->prev;
	}
}

void coalese_all(void)
{
	struct block_meta *curr = global_start;

	while (curr) {
		if (curr->status == STATUS_FREE)
			coalesce(&curr);

		curr = curr->next;
	}
}

void split_block(struct block_meta **block, size_t size)
{
	size_t size_mem_block = ALIGN(size) + SIZE_BLOCK_META;

	if ((*block)->size >= size_mem_block + ALIGN(MIN_BLOCK_SIZE)) {
		struct block_meta *new_block = (struct block_meta *)((char *)(*block) + size_mem_block);

		new_block->size = (*block)->size - size_mem_block;
		new_block->status = STATUS_FREE;
		new_block->prev = (*block);
		new_block->next = (*block)->next;

		(*block)->size = ALIGN(size);
		(*block)->status = STATUS_ALLOC;
		(*block)->next = new_block;
	} else {
		(*block)->status = STATUS_ALLOC;
	}
}

struct block_meta *find_fit(struct block_meta **last, size_t size)
{
	struct block_meta *curr = global_start;

	while (curr && !(curr->status == STATUS_FREE && curr->size >= size)) {
		*last = curr;
		curr = curr->next;
	}

	return curr;
}

void *heap_prealloc(size_t size)
{
	void *mem = sbrk(size);

	if (mem == (void *)-1) {
		DIE(mem == (void *)-1, "heap_prealloc");
		return NULL;
	}

	global_start = (struct block_meta *)mem;
	global_start->size = size - SIZE_BLOCK_META;
	global_start->status = STATUS_ALLOC;
	global_start->prev = NULL;
	global_start->next = NULL;

	void *payload = (char *)global_start + SIZE_BLOCK_META;

	return payload;
}

void *extend_block(struct block_meta *block, size_t size)
{
	void *mem = sbrk(ALIGN(size) - block->size);

	if (mem == (void *)-1) {
		DIE(mem == (void *)-1, "extend_block");
		return NULL;
	}

	block->size = ALIGN(size);
	block->status = STATUS_ALLOC;

	return mem;
}

void *os_malloc(size_t size)
{
	/* TODO: Implement os_malloc */

	if (size <= 0)
		return NULL;

	size_t size_mem_block = ALIGN(size) + SIZE_BLOCK_META;
	size_t size_block_meta = ALIGN(size);

	if (size_mem_block <= global_treshold) {
		if (global_status == STATUS_FREE) {
			void *mem = heap_prealloc(MMAP_THRESHOLD);

			if (mem == NULL) {
				DIE(mem == NULL, "os_malloc");
				return NULL;
			}

			global_status = STATUS_ALLOC;

			return mem;
		}
		// global_status == STATUS_ALLOC
		struct block_meta *last = global_start;
		struct block_meta *block = find_fit(&last, size_block_meta);

		if (block) {
			split_block(&block, size);

			void *payload = (char *)block + SIZE_BLOCK_META;

			return payload;

		} else { // block == NULL
			if (last->status == STATUS_FREE) {
				void *mem = extend_block(last, size);

				if (mem == NULL) {
					DIE(mem == NULL, "extend_block");
					return NULL;
				}

				void *payload = (char *)last + SIZE_BLOCK_META;

				return payload;
			}

			void *mem = sbrk(size_mem_block);

			if (mem == (void *)-1) {
				DIE(mem == (void *)-1, "os_malloc");
				return NULL;
			}

			block = (struct block_meta *)mem;
			asign_block(&block, size_block_meta, STATUS_ALLOC, last, NULL);
			last->next = block;

			void *payload = (char *)block + SIZE_BLOCK_META;

			return payload;
		}
	} else { // size_mem_block > MMAP_THRESHOLD
		void *mem = mmap(NULL, size_mem_block, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

		if (mem == MAP_FAILED) {
			DIE(mem == MAP_FAILED, "os_malloc");
			return NULL;
		}

		if (!global_start)
			global_start = (struct block_meta *)mem;

		struct block_meta *block = (struct block_meta *)mem;

		asign_block(&block, size_block_meta, STATUS_MAPPED, NULL, NULL);

		void *payload = (char *)block + SIZE_BLOCK_META;

		return payload;
	}
	return NULL;
}

void os_free(void *ptr)
{
	/* TODO: Implement os_free */

	if (!ptr)
		return;

	struct block_meta *block = (struct block_meta *)((char *)ptr - SIZE_BLOCK_META);

	if (block->status == STATUS_ALLOC) {
		block->status = STATUS_FREE;
		coalese_all();

	} else if (block->status == STATUS_MAPPED) {
		block->status = STATUS_FREE;
		munmap(block, block->size + SIZE_BLOCK_META);
	}
}

void *os_calloc(size_t nmemb, size_t size)
{
	/* TODO: Implement os_calloc */

	// if (nmemb <= 0 || size <= 0)
	// 	return NULL;

	// size_t total_size = nmemb * size;

	// global_treshold = PAGE_SIZE;
	// void *mem = os_malloc(total_size);

	// global_treshold = MMAP_THRESHOLD;

	// memset(mem, 0, total_size);

	// return mem;
}

void *os_realloc(void *ptr, size_t size)
{
	/* TODO: Implement os_realloc */

	// if (!ptr)
	// 	return os_malloc(size);

	// if (size <= 0) {
	// 	os_free(ptr);
	// 	return NULL;
	// }

	// struct block_meta *block = (struct block_meta *)((char *)ptr - SIZE_BLOCK_META);
	// size_t size_block_meta = ALIGN(size);

	// coalese_all();

	// if (block->status == STATUS_FREE)
	// 	return NULL;

	// if (block->status == STATUS_MAPPED) {
	// 	//global_status = STATUS_FREE;
	// 	void *new_mem = os_malloc(size);

	// 	if (new_mem == NULL) {
	// 		DIE(new_mem == NULL, "os_realloc");
	// 		return NULL;
	// 	}

	// 	if (block->size < size_block_meta)
	// 		memcpy(new_mem, ptr, block->size);
	// 	else
	// 		memcpy(new_mem, ptr, size_block_meta);

	// 	os_free(ptr);

	// 	return new_mem;
	// }

	// // block->status == STATUS_ALLOC

	// if (block->size >= size_block_meta) {
	// 	split_block(&block, size);

	// 	return ptr;
	// }

	// if (!block->next) {
	// 	void *new_mem = extend_block(block, size);

	// 	if (new_mem == NULL) {
	// 		DIE(new_mem == NULL, "os_realloc");
	// 		return NULL;
	// 	}

	// 	return ptr;
	// }

	// if (block->next->status == STATUS_FREE) {
	// 	if (block->size + block->next->size + SIZE_BLOCK_META >= size_block_meta) {
	// 		coalesce(&block);
	// 		split_block(&block, size);

	// 		return ptr;
	// 	}
	// }

	// if (block->size < size_block_meta) {
	// 	void *new_mem = os_malloc(size);

	// 	if (new_mem == NULL) {
	// 		DIE(new_mem == NULL, "os_realloc");
	// 		return NULL;
	// 	}

	// 	memcpy(new_mem, ptr, block->size);

	// 	os_free(ptr);

	// 	return new_mem;
	// }

	return NULL;
}