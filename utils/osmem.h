/* SPDX-License-Identifier: BSD-3-Clause */

#pragma once

#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include "printf.h"

#define MMAP_THRESHOLD (128 * 1024)
#define MAP_ANONYMOUS 0x20
#define PAGE_SIZE 4080
#define ALIGN_BYTE(size) ((size + 7) & (~7))
#define BLOCK_SIZE sizeof(struct block_meta)

void *os_malloc(size_t size);
void os_free(void *ptr);
void *os_calloc(size_t nmemb, size_t size);
void *os_realloc(void *ptr, size_t size);
