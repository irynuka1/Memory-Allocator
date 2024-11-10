/* SPDX-License-Identifier: BSD-3-Clause */

/* MMAP and SBRK defines*/
#define PROT_READ 0x1  /* Page can be read.  */
#define PROT_WRITE 0x2 /* Page can be written.  */
#define PROT_EXEC 0x4  /* Page can be executed.  */
#define PROT_NONE 0x0  /* Page can not be accessed.  */

/* Sharing types (must choose one and only one of these).  */
#define MAP_SHARED 0x01    /* Share changes.  */
#define MAP_PRIVATE 0x02   /* Changes are private.  */
#define MAP_ANONYMOUS 0x20 /* Don't use a file.  */
#define MAP_ANON MAP_ANONYMOUS

#define MREMAP_MAYMOVE 1
#define MMAP_THRESHOLD (128 * 1024)

/* Struct block_meta defines */
// Source: https://moss.cs.iit.edu/cs351/slides/slides-malloc.pdf
#define ALIGNMENT 8
#define ALIGN(size) (((size) + (ALIGNMENT - 1)) & ~(ALIGNMENT - 1))
#define SIZE_BLOCK_META (sizeof(struct block_meta))

#define MIN_BLOCK_SIZE 1
#define PAGE_SIZE 4 * 1024