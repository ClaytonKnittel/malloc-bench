/*
 ******************************************************************************
 *                               mm.c                                         *
 *           64-bit struct-based implicit free list memory allocator          *
 *                      without coalesce functionality                        *
 *                 CSE 361: Introduction to Computer Systems                  *
 *                                                                            *
 *  ************************************************************************  *
 *                     insert your documentation here. :)                     *
 *                                                                            *
 *  ************************************************************************  *
 *  ** ADVICE FOR STUDENTS. **                                                *
 *  Step 0: Please read the writeup!                                          *
 *  Step 1: Write your heap checker. Write. Heap. checker.                    *
 *  Step 2: Place your contracts / debugging assert statements.               *
 *  Good luck, and have fun!                                                  *
 *                                                                            *
 ******************************************************************************
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stddef.h>
#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <mmintrin.h>

#include "mm.h"
#include "memlib.h"

#ifdef DRIVER
/* create aliases for driver tests */
#define malloc mm_malloc
#define free mm_free
#define realloc mm_realloc
#define calloc mm_calloc
#endif /* def DRIVER */


//#define DEBUG
//#define DEFINE_CHECKS
//#define DO_PRINT
//#define DO_SPEAK


#ifdef DO_SPEAK
#define speak(...) printf(__VA_ARGS__)
#else
#define speak(...)
#endif

//#define inline
//#define static static __attribute__((noinline))

typedef uint64_t ptr_int_t;


// Color printing!!

#define P_RESET   "\033[0m"

#define P_BLACK   "\033[0;30m"
#define P_RED     "\033[0;31m"
#define P_GREEN   "\033[0;32m"
#define P_YELLOW  "\033[0;33m"
#define P_BLUE    "\033[0;34m"
#define P_MAGENTA "\033[0;35m"
#define P_CYAN    "\033[0;36m"
#define P_WHITE   "\033[0;37m"
#define P_DEFAULT "\033[0;39m"

#define P_LGRAY    "\033[0;37m"
#define P_DGRAY    "\033[0;90m"
#define P_LRED     "\033[0;91m"
#define P_LGREEN   "\033[0;92m"
#define P_LYELLOW  "\033[0;93m"
#define P_LBLUE    "\033[0;94m"
#define P_LMAGENTA "\033[0;95m"
#define P_LCYAN    "\033[0;96m"
#define P_LWHITE   "\033[0;97m"

#define BOLD    "\033[1m"
#define NORMAL  "\033[21m"


struct slab;

#ifdef DEFINE_CHECKS
static void print_heap();
static void print_slab(struct slab * s);
#endif


#define _STR(s) #s
#define STR(s) _STR(s)

#ifdef DO_PRINT
#define MALLOC_ASSERT(expr) \
	do { \
		if (__builtin_expect(!(expr), 0)) { \
			print_heap(); \
			fprintf(stderr, P_RED "Assertion failed" P_DEFAULT " on line " \
					STR(__LINE__) ": %s\n", #expr); \
			abort(); \
		} \
	} while (0)
#else
#define MALLOC_ASSERT(expr) \
	do { \
		if (__builtin_expect(!(expr), 0)) { \
			fprintf(stderr, P_RED "Assertion failed" P_DEFAULT " on line " \
					STR(__LINE__) ": %s\n", #expr); \
			abort(); \
		} \
	} while (0)
#endif /* DO_PRINT */

#define MALLOC_ASSUME(expr) \
	do { \
		if (!(expr)) { \
			__builtin_unreachable(); \
		} \
	} while (0)


#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))

// aligns value down by power of two mod
#define ALIGN_DOWN(val, mod) \
	((val) & ~((mod) - 1))

// aligns value up by power of two mod
#define ALIGN_UP(val, mod) \
	(((val) + (mod) - 1) & ~((mod) - 1))

// ceiling divides val by power of two mod
#define CEIL_DIV(val, mod) \
	(((val) + (mod) - 1) / (mod))



/*
 * for multiples of PAGESIZE allocations, pointers will be aligned on a page
 * bonudary
 */

/*
 * align pointers by 16 bytes
 */
#define MALLOC_ALIGN 16
#define MALLOC_ALIGN_SHIFT 4


#define SLAB_SIZE 4096
#define SLAB_SIZE_SHIFT 12
#define SLAB_SIZE_MASK (~(SLAB_SIZE - 1))


/*
 * see struct slab definition for descriptions of the flags
 */
#define SLAB_ALLOC_BIT      0x1
#define SLAB_PREV_ALLOC_BIT 0x2
#define SLAB_PACKED_BIT     0x4

/*
 * see struct heap definition for description of the flags
 */
#define LAST_SLAB_ALLOC 0x1



/*
 * blocks this size and smaller need a 2-level bitvector
 */
#define PACKED_SLAB_2LVL_BITV_THRESH 48


/*
 * lengths in bytes of the respective bitvectors of each size class
 * (second level only)
 */
#define PACKED_SLAB_16_BITV_LEN 32
#define PACKED_SLAB_32_BITV_LEN 16
#define PACKED_SLAB_48_BITV_LEN 11

#define PACKED_SLAB_16_HEADER_SIZE \
	ALIGN_UP((24 + PACKED_SLAB_16_BITV_LEN), MALLOC_ALIGN)
#define PACKED_SLAB_32_HEADER_SIZE \
	ALIGN_UP((24 + PACKED_SLAB_32_BITV_LEN), MALLOC_ALIGN)
#define PACKED_SLAB_48_HEADER_SIZE \
	ALIGN_UP((24 + PACKED_SLAB_48_BITV_LEN), MALLOC_ALIGN)

/*
 * lengths in bits of the respective bitvectors of each size class
 * (second level only)
 */
#define PACKED_SLAB_16_BITV_BITS \
	((SLAB_SIZE - PACKED_SLAB_16_HEADER_SIZE) / 16)
#define PACKED_SLAB_32_BITV_BITS \
	((SLAB_SIZE - PACKED_SLAB_32_HEADER_SIZE) / 32)
#define PACKED_SLAB_48_BITV_BITS \
	((SLAB_SIZE - PACKED_SLAB_48_HEADER_SIZE) / 48)

// make sure the above lengths fit exactly in the headers
static_assert(CEIL_DIV(PACKED_SLAB_16_BITV_BITS, 8) == PACKED_SLAB_16_BITV_LEN,
		"16 byte packed slab bitv does not fit in the header");
static_assert(CEIL_DIV(PACKED_SLAB_32_BITV_BITS, 8) == PACKED_SLAB_32_BITV_LEN,
		"32 byte packed slab bitv does not fit in the header");
static_assert(CEIL_DIV(PACKED_SLAB_48_BITV_BITS, 8) == PACKED_SLAB_48_BITV_LEN,
		"48 byte packed slab bitv does not fit in the header");


// size of the header of packed slabs of size >= 64
#define PACKED_SLAB_HEADER_SIZE 32

/*
 * for allocations below this threshold, have dedicated blocks to each
 * individual size (by multiples of 16)
 */
#define MAX_TINY_BLOCK_SZ 496

/*
 * medium blocks are all blocks that aren't placed into packed slabs but can
 * still fit within a single slab (including metadata)
 */
#define MIN_MEDIUM_BLOCK_SZ (MAX_TINY_BLOCK_SZ + MALLOC_ALIGN)
#define MAX_MEDIUM_BLOCK_SZ (SLAB_SIZE - MALLOC_ALIGN)

/*
 * large blocks are all blocks that are too big to fit in a single slab, and
 * are therefore placed at the end of a contiguous region of slabs
 */
#define MIN_LARGE_BLOCK_SZ (SLAB_SIZE)

#define LARGE_SLAB_HEADER_SIZE (offsetof(slab_t, large_slab_payload_start))

/*
 * when reallocing a large block, the maximum amount of deadweight space that
 * is allowed to be at the end before forcing malloc/copy/free
 * i.e. if you realloc a 4096 byte block to 4000, deadweight would be 96 bytes,
 * as this space is not allocatable until the block is freed
 */
#define REALLOC_MAX_DEADWEIGHT MAX_TINY_BLOCK_SZ

/*
 * keep a segregated list of free slabs divided into segregated lists of sizes
 * up to 127, after which all larger slabs go in a single bin (just the last
 * bin in this list)
 */
#define NUM_SLAB_BINS 128
#define NUM_SEG_SLAB_BINS (NUM_SLAB_BINS - 1)

/*
 * size of the largest slab bins which gets its own dedicated slab bin
 * (everything above this just goes in the final largest slab bin)
 */
#define MAX_SEG_SLAB_SZ NUM_SEG_SLAB_BINS

#define SLAB_BINS_SKIPLIST_SZ \
	((NUM_SLAB_BINS + (8 * sizeof(uint32_t) - 1)) / (8 * sizeof(uint32_t)))

/*
 * smallbins are doubly linked lists of size classes of packed slabs, with
 * one for each size class
 *
 * we subtract 8 from this because we combine a total of 8 size classes
 * together
 */
#define NUM_SMALLBINS ((MAX_TINY_BLOCK_SZ / MALLOC_ALIGN) - 8)

/*
 * mediumbins are doubly-linked lists of size classes of free blocks within
 * large slabs
 */
#define NUM_MEDIUMBINS \
	(((MAX_MEDIUM_BLOCK_SZ - MIN_MEDIUM_BLOCK_SZ) / MALLOC_ALIGN) + 1)

/*
 * number of uint32_t's needed in the skiplist for medium blocks (one bit
 * per bin)
 */
#define MEDIUMBINS_SKIPLIST_SZ \
	((NUM_MEDIUMBINS + (8 * sizeof(uint32_t) - 1)) / (8 * sizeof(uint32_t)))




/*
 * slabs are guaranteed to be aligned by PAGESIZE bytes
 */
typedef struct __attribute__((packed)) slab {
	/*
	 * flags structure
	 *
	 *   0   1   2 - 7
	 * +---+---+------+
	 * | a | p | .... |
	 * +---+---+------+
	 *
	 * where:
	 *  a is set when the slab has allocated blocks in it
	 *  p is set when the previous slab is allocated
	 *
	 *
	 *   allocated state:
	 *
	 *     0   1   2   3 - 7
	 *   +---+---+---+------+
	 *   | 1 | p | t | .... |
	 *   +---+---+---+------+
	 *
	 *   where:
	 *     t is set when this is a tiny block slab
	 *
	 *
	 *     allocated tiny block slab:
	 *
	 *       0   1   2   3  ...  7
	 *     +---+---+---+----------+
	 *     | 1 | p | 1 |          |
	 *     +---+---+---+----------+
	 *
	 *     where:
	 *       sz_class: describes the sizes of blocks in this tiny block slab
	 *           block_size = (16 * sz_class) + 16
	 *
	 *
	 *     allocated normal block slab:
	 *
	 *       0   1   2   3  ...  7
	 *     +---+---+---+----------+
	 *     | 1 | p | 0 |          |
	 *     +---+---+---+----------+
	 *
	 *
	 *   freed state:
	 *
	 *     0   1   2 - 7
	 *   +---+---+------+
	 *   | 0 | p | 0... |
	 *   +---+---+------+
	 *
	 *
	 */
	uint8_t flags;

	union __attribute__((packed)) {
		char data[SLAB_SIZE - sizeof(uint8_t)];

		// freed slab
		struct __attribute__((packed)) {
			// align pointers
			char __padding0[7];

			// keep freed slabs in powers of 2 size bins which are kept sorted
			struct slab * next;
			struct slab * prev;

			// number of contiguous free slabs starting from this one
			uint64_t num_slabs;

			// the footer takes up the last 8 bytes of the slab, wherever that
			// may be, and ensures that its value is alwayas equal to this
			// slab's size
		};

		// allocated slabs:
		union {
			// packed slab
			union {
				/*
				 * For the smaller chunk sizes, use a two-level bitvector to
				 * track free blocks within the slab. The lower level bitvector
				 * has one bit corresponding to each allocatable blocks within
				 * the region, with bit 0 of byte 0 corresponding to the blcok
				 * at offset 0 from the start of data, continuing from there.
				 * The higher level bitvector has one bit corresponding to each
				 * of the lower level bitvectors, only being 0 when the
				 * corresponding full (all 0's)
				 *
				 * For the larger chunk sizes, which have <= 64 blocks fitting in
				 * the region (size >= 64), use a single flat bitvector
				 */

				// smaller chunk sizes (16 - 48) with 2-level bitvector
				struct __attribute__((packed)) {
					// (0 - 2), for 16 - 48
					uint8_t sz_class;

					/*
					 * a count of the number of allocated blocks in this slab
					 */
					uint16_t alloc_cnt;

					/*
					 * level 1 bitvector: bit set if corresponding bitvector
					 * is not full (fully allocated)
					 * all 16 bits are used by 16 byte packed slabs,
					 * only first 8 are used by 32 byte,
					 * only first 7 are used by 48 byte
					 */
					uint16_t ps_f1;

					char __padding3[2];

					struct slab * next_p;
					struct slab * prev_p;

					// level 2 bitvectors: bit set if corresponding block is
					// free
					uint16_t ps_f2[0];
				};

				// larger chunk sizes (64+) with a 1-level bitvector
				struct __attribute__((packed)) {
					// aliases sz_class
					// (3 - 31), for 64 - 512
					uint8_t __sz_class_alias;

					uint16_t __alloc_cnt_alias;

					// align bitvector
					char __padding4[4];

					struct slab * __next_p_alias;
					struct slab * __prev_p_alias;

					// single bitvector, with bits set to 1 when the
					// corresponding blocks are free
					uint64_t pl_f;
				};
			};

			// large slab
			struct __attribute__((packed)) {
				/*
				 * the next 7 bytes are used for the offsets of blocks within
				 * this slab, including >= 4096-sizes blocks hanging past the
				 * end of this slab. There can only be at most 8 such blocks,
				 * since each block must be greater than 512 bytes (and there
				 * may be a large block at the end). Also, each block has to be
				 * between 528-4080 bytes, so only 8 bits are necessary to
				 * store each size
				 *
				 * The 0'th offset is implied to be 16 (i.e. 2), meaning we
				 * store the offsets of all but the first block
				 *
				 * an offset of 0 denotes the end (i.e. it continues to the
				 * end of the last slab owned by this one). If there are 7
				 * blocks, then there is no 0 byte and block 7 is assumed to
				 * go to the end
				 *
				 * TODO maybe make this 15 elements and allow sizes down to 256
				 */
				uint8_t block_offs[7];

				union {
					/*
					 * offset is the number of contiguous slabs this slab owns
					 *
					 * addressable memory is <= 64 bits, and offsets are in
					 * multiples of 4096, leaving 36 bits of information
					 * necessary
					 *
					 * if offset is not 1, then this slab is assumed to have an
					 * allocated large block hanging over the edge of this slab
					 * and filling the next (offset - 1) slabs
					 */
					uint64_t offset;

					struct {
						uint8_t __padding5[7];

						/*
						 * bitvector of alloc bits for the (up to) 7 smaller
						 * blocks in this slab, takes up the highest order byte
						 * of offset, since offset can't exceed 2 ^ 52
						 *
						 * a set bit indicates an allocated block (note that
						 * this is opposite of the bitvectors for packed
						 * blocks)
						 *
						 * if this slab contains a large block extending beyond
						 * the end, the bit in block_alloc where the 0x00's
						 * start in block_offs must be set. This is used to
						 * disambiguate large blocks extending from the last
						 * offset from large blocks which are multiples of
						 * SLAB_SIZE and start at offset SLAB_SIZE (which means
						 * their offset is 0x00 in the offset vector, as these
						 * offsets are only 1 byte)
						 */
						uint8_t  block_alloc;
					};
				};

				// used to calculate the size of large block header
				char large_slab_payload_start[0];
			};
		};
	};

	// since slabs are PAGESIZE aligned, and this field aliases the
	// next slab pointer, this field is free to take on any value
} slab_t;


// sanity check
static_assert(sizeof(slab_t) == SLAB_SIZE, "the slab struct is not the "
		"expected size");

// ensure that sz_class alias is indeed an alias
static_assert(offsetof(slab_t, sz_class) == offsetof(slab_t, __sz_class_alias),
		"sz_class is not aliased properly");
// ensure that alloc_cnt alias is indeed an alias
static_assert(offsetof(slab_t, alloc_cnt) == offsetof(slab_t, __alloc_cnt_alias),
		"sz_class is not aliased properly");

// ensure that the prev_p/next_p aliases are indeed aliases
static_assert(offsetof(slab_t, next_p) == offsetof(slab_t, __next_p_alias),
		"next_p is not aliased properly");
static_assert(offsetof(slab_t, prev_p) == offsetof(slab_t, __prev_p_alias),
		"prev_p is not aliased properly");



/*
 * struct for medium-sized blocks in a slab, only kept in the medium bins
 * when free. Since all metadata is recorded at the head of the slab, no
 * metadata is kept by the memory itself
 */
typedef struct block
{
	union {
		struct block * next;
		struct block * head;
	};
	union {
		struct block * prev;
		struct block * tail;
	};
} block_t;

/*
 * struct for slab bin lists, which are fully circularly linked, as the bins
 * are FIFO
 */
typedef struct slab_bin
{
	slab_t * head;
	slab_t * tail;
} slab_bin_t;



typedef struct heap
{
	// powers of 2 seglists of unallocated slabs (NULL terminated, rather than
	// circular linked lists)
	slab_bin_t slab_bins[NUM_SLAB_BINS];

	// size bins of small packed slabs from 16 - 512 (NULL terminated)
	slab_t * smallbins[NUM_SMALLBINS];

	// doubly-linked circular size bins of medium blocks from 528 - 4078
	// block_t nicely doubles as the head/tail struct for the roots of the
	// lists
	block_t * mediumbins[NUM_MEDIUMBINS];

	uint32_t slab_skiplist[SLAB_BINS_SKIPLIST_SZ];

	// bitvector of medium bins with > 0 elements (to allow for quickly
	// skipping through empty bins)
	uint32_t med_skiplist[MEDIUMBINS_SKIPLIST_SZ];

	/*
	 * a pointer to just beyond the very end of the heap (i.e. a pointer to the
	 * next slab to be allocated by mem_sbrk)
	 */
	slab_t * heap_end;

	/*
	 * status flags for the heap:
	 *  LAST_SLAB_ALLOC: set when the last slab in the heap is allocated
	 */
	int flags;

} heap_t;


//static_assert(sizeof(heap_t) <= SLAB_SIZE, "heap does not fit in 1 slab");



/* Global variables */
/* Pointer to first block */
static heap_t * heap = NULL;



/*
 * --------------------- slab helper functions ---------------------
 */

static inline int slab_is_free(slab_t * s) {
	return !(s->flags & SLAB_ALLOC_BIT);
}

static inline int slab_is_alloc(slab_t * s) {
	return s->flags & SLAB_ALLOC_BIT;
}

static inline int slab_is_packed(slab_t * s) {
	return s->flags & SLAB_PACKED_BIT;
}

// returns the slab associated with the pointer
static inline slab_t * ptr_get_slab(void * ptr) {
	return (slab_t *) ALIGN_DOWN((((ptr_int_t) ptr) - MALLOC_ALIGN), SLAB_SIZE);
}

static inline slab_t * block_get_slab(block_t * b) {
	return (slab_t *) ALIGN_DOWN((ptr_int_t) b, SLAB_SIZE);
}


/*
 * returns the index of the slab bin which would hold a slab region of n_slabs
 */
static inline uint8_t slab_bin_idx(uint64_t n_slabs) {
	return MIN(n_slabs - 1, NUM_SLAB_BINS - 1);
}
/*
 * returns the index of the slab bin which would hold a slab region of n_slabs,
 * assuming n_slabs < NUM_SLAB_BINS
 */
static inline uint8_t slab_bin_idx_small(uint64_t n_slabs) {
	return n_slabs - 1;
}

/*
 * in the following two methods, we are safe to "downgrade" bin_idx to a 32-bit
 * integer since we know it will never get that large, and won't require zeroing
 * out the upper half of its register
 */
static inline uint8_t slab_bin_skiplist_idx(uint32_t bin_idx) {
	return bin_idx / (8 * sizeof(uint32_t));
}

static inline uint8_t slab_bin_skiplist_bit(uint32_t bin_idx) {
	return bin_idx & ((8 * sizeof(uint32_t)) - 1);
}


static inline uint64_t free_slab_size(slab_t * s) {
	return s->num_slabs;
}

/*
 * returns the number of slabs necessary to store a block of the given size
 */
static inline uint64_t req_slabs_for_size(uint64_t size) {
	return ALIGN_UP(size + LARGE_SLAB_HEADER_SIZE, SLAB_SIZE) / SLAB_SIZE;
}

static inline uint64_t large_slab_get_size(slab_t * s) {
	uint64_t offset = s->offset;
	return offset & 0x00ffffffffffffffLU;
}

static inline void large_slab_set_size(slab_t * s, uint64_t size) {
	uint64_t block_alloc;
	__asm__(
			"movb %b[block_alloc],%b[ba]\n\t"
			"shl  $0x38,%[ba]\n\t"
			"or   %[size],%[ba]\n"
			: [ba] "=&r" (block_alloc)
			: [block_alloc] "rm" (s->block_alloc),
			  [size] "ri" (size));
	s->offset = block_alloc;
}

/*
 * retusn the size of the slab (in units of SLAB_SIZE bytes)
 */
static inline uint64_t slab_get_size(slab_t * s) {
	return slab_is_free(s) ? s->num_slabs :
		slab_is_packed(s) ? 1 : large_slab_get_size(s);
}


static inline slab_t * next_adj_slab(slab_t * s) {
	return s + slab_get_size(s);
}

static inline slab_t * prev_adj_slab(slab_t * s) {
	uint64_t * footer = ((uint64_t *) s) - 1;
	return s - (*footer);
}


static inline slab_t * __slab_bin_start(slab_bin_t * slab_bin) {
	return (slab_t *) (((ptr_int_t) slab_bin) - offsetof(slab_t, next));
}


/*
 * inserts the slab into the given slab bin
 */
static inline void __slab_link(slab_bin_t * slab_bin, slab_t * s) {
	// set prev to point to the right address so that when the "next" field
	// of the slab is accessed, you get the bin itself
	slab_t * prev = slab_bin->tail;
	slab_t * next = __slab_bin_start(slab_bin);

	s->next = next;
	s->prev = prev;
	slab_bin->tail = s;
	prev->next = s;
}

static inline void __slab_link_large(slab_bin_t * slab_bin, slab_t * s, uint64_t n_slabs) {
	slab_t * prev = slab_bin->tail;
	slab_t * bin_start = __slab_bin_start(slab_bin);
	slab_t * next = bin_start;

	while (prev != bin_start && prev->num_slabs > n_slabs) {
		next = prev;
		prev = prev->prev;
	}

	s->next = next;
	s->prev = prev;
	//slab_bin->tail = s;
	next->prev = s;
	prev->next = s;
}

/*
 * links slab in appropriate bin, assuming s is a small bin
 * (i.e. n_slabs <= MAX_SEG_SLAB_SIZE)
 */
static inline void slab_link_small(heap_t * h, slab_t * s, uint64_t n_slabs) {
	uint8_t bin_idx = slab_bin_idx_small(n_slabs);
	__slab_link(&h->slab_bins[bin_idx], s);

	// update skiplist
	uint8_t skip_idx = slab_bin_skiplist_idx(bin_idx);
	uint8_t bit_idx  = slab_bin_skiplist_bit(bin_idx);
	h->slab_skiplist[skip_idx] |= (1U << bit_idx);
}

/*
 * inserts the slab into the correct slab bin, where size must match the
 * num_slabs field of s
 */
static inline void slab_link(heap_t * h, slab_t * s, uint64_t size) {
	uint8_t bin_idx = slab_bin_idx(size);

	if (size <= MAX_SEG_SLAB_SZ) {
		__slab_link(&h->slab_bins[bin_idx], s);
		// update skiplist
		uint8_t skip_idx = slab_bin_skiplist_idx(bin_idx);
		uint8_t bit_idx  = slab_bin_skiplist_bit(bin_idx);
		// don't mark the the last slab bin in the skiplist
		h->slab_skiplist[skip_idx] |= (1u << bit_idx);
	}
	else {
		__slab_link_large(&h->slab_bins[NUM_SLAB_BINS - 1],
				s, size);
	}
}

/*
 * removes the slab from its freelist
 */
static inline void slab_unlink(slab_t * s) {
	slab_t * next = s->next;
	slab_t * prev = s->prev;
	prev->next = next;
	next->prev = prev;
}



/*
 * --------------------- tiny block helper functions ---------------------
 */

static inline uint8_t is_tiny_block_size(size_t size) {
	return size <= MAX_TINY_BLOCK_SZ;
}

/*
 * adjusts the tiny block size, grouping together sizes which fit the same
 * number of blocks in a slab
 */
static inline uint32_t adj_tiny_size(uint32_t size) {
	return size < 320 ? (size + ((size == 256) << 4)) :
		size < 416 ? ((size & ~0x1f) + 16) :
		(400 + 3 * (((size + 48) >> 4) & ~0xf));
}

static inline uint8_t packed_slab_sz_class(slab_t * s) {
	return s->sz_class;
}

static inline uint32_t packed_slab_block_size(slab_t * s) {
	return MALLOC_ALIGN + (((uint32_t) s->sz_class) * MALLOC_ALIGN);
}

static inline uint8_t size_to_sz_class(uint32_t size) {
	// TODO combine sizes which all fit same number of blocks into one size
	// class, where all blocks in the slab are the same size
	// TODO fix this to match packed_bin_idx
	return (size / MALLOC_ALIGN) - 1;
}

static inline uint32_t sz_class_to_size(uint8_t sz_class) {
	return (((uint32_t) sz_class) + 1) * MALLOC_ALIGN;
}

static inline uint8_t packed_bin_idx(uint32_t size) {
	return size < 256 ? ((size / MALLOC_ALIGN) - 1) :
		size < 368 ? ((size * 3 / (4 * MALLOC_ALIGN)) + 3) :
		(((size * 3 + 32) / (8 * MALLOC_ALIGN)) + 11);
}

// returns the number of blocks that fit into a packed slab of a given size
static inline uint32_t packed_slab_n_blocks(uint32_t size) {
	return (SLAB_SIZE - PACKED_SLAB_HEADER_SIZE) / size;
}


static uint8_t packed_slab_is_empty(slab_t * s) {
	return s->alloc_cnt == 0;
}


/*
 * for slabs with a 2-level bitvector, checking whether it is full is simply a
 * matter of checking if the first level bitvector is empty
 */
static int __packed_slab_2lvl_is_full(slab_t * s) {
	return s->ps_f1 == 0;
}

#define packed_slab_16_is_full(s) \
	__packed_slab_2lvl_is_full(s)
#define packed_slab_32_is_full(s) \
	__packed_slab_2lvl_is_full(s)
#define packed_slab_48_is_full(s) \
	__packed_slab_2lvl_is_full(s)

/*
 * returns whether the given slab of given size (which must agree with
 * s->sz_class) is full (i.e. contains no free blocks)
 */
static inline int packed_slab_is_full(slab_t * s) {
	return s->pl_f == 0;
}


// forward declared since it's used below
static inline void small_bin_link(heap_t * h, slab_t * s, uint32_t size);
static inline void small_bin_unlink(slab_t * s);
static void __int_free_slab(heap_t * h, slab_t * s, uint64_t n_slabs);
static inline void __int_free_remainder_slab(heap_t * h, slab_t * s, uint64_t n_slabs);

/*
 * allocates an N-byte block from a packed slab, assuming s is a packed slab
 * of the correct size class and contains at least one free block
 *
 * N must be 16, 32, or 48
 */
#define __DEFINE_PACKED_ALLOC_N(N) \
static inline void *__packed_alloc_ ## N(slab_t * s) {	\
	uint16_t l1_bitv = s->ps_f1;						\
	uint32_t l1_idx = __builtin_ctz(l1_bitv);			\
	uint16_t l2_bitv = s->ps_f2[l1_idx];				\
	uint32_t l2_idx = __builtin_ctz(l2_bitv);			\
	uint32_t idx = l1_idx * 16 + l2_idx;				\
	\
	l2_bitv ^= 1u << l2_idx;							\
	l1_bitv ^= (!l2_bitv) << l1_idx;					\
	s->alloc_cnt++;										\
	s->ps_f2[l1_idx] = l2_bitv;							\
	s->ps_f1 = l1_bitv;									\
	\
	uint32_t offset = (idx * N) + 						\
			PACKED_SLAB_##N##_HEADER_SIZE;				\
	\
	if (l1_bitv == 0) {									\
		small_bin_unlink(s);							\
	}													\
	\
	return (void *) (((ptr_int_t) s) + offset);			\
}

// define for the 3 size classes with 2-level bitvectors
__DEFINE_PACKED_ALLOC_N(16);
__DEFINE_PACKED_ALLOC_N(32);
__DEFINE_PACKED_ALLOC_N(48);

/*
 * the general case of packed_alloc for a packed slab
 */
static inline void *__packed_alloc(slab_t * s, uint32_t size) {
	uint64_t bitv = s->pl_f;
	uint32_t idx = __builtin_ctzl(bitv);
	bitv ^= (1LU << idx);
	if (bitv == 0) {
		small_bin_unlink(s);
	}
	s->alloc_cnt++;
	s->pl_f = bitv;

	uint32_t offset = (idx * size) + PACKED_SLAB_HEADER_SIZE;
	return (void *) (((ptr_int_t) s) + offset);
}


/*
 * frees a ptr into a packed slab with a 2-level bitvector
 */
#define __DEFINE_PACKED_FREE_N(N) \
static inline void __packed_free_ ## N(heap_t * h, slab_t * s,	\
		void * ptr) {											\
	uint32_t idx = (uint32_t)									\
			((((ptr_int_t) ptr) - ((ptr_int_t) s) -				\
			  PACKED_SLAB_##N##_HEADER_SIZE) / N);				\
	uint32_t l1_idx = idx >> 4;									\
	uint32_t l2_idx = idx & 0xf;								\
	uint16_t l1_bitv = s->ps_f1;								\
	uint16_t l2_bitv = s->ps_f2[l1_idx];						\
	uint16_t alloc_cnt = s->alloc_cnt - 1;						\
	if (l1_bitv == 0) {											\
		small_bin_link(h, s, N);								\
	}															\
	l1_bitv |= (1u << l1_idx);									\
	l2_bitv |= (1u << l2_idx);									\
	s->alloc_cnt = alloc_cnt;									\
	s->ps_f2[l1_idx] = l2_bitv;									\
	s->ps_f1 = l1_bitv;											\
	if (alloc_cnt == 0) {										\
		small_bin_unlink(s);									\
		__int_free_slab(h, s, 1);								\
	}															\
}

// define for the 3 size classes with 2-level bitvectors
__DEFINE_PACKED_FREE_N(16);
__DEFINE_PACKED_FREE_N(32);
__DEFINE_PACKED_FREE_N(48);

/*
 * the general case of packed_free for a packed slab
 */
static inline void __packed_free(heap_t * h, slab_t * s, void * ptr,
		uint8_t sz_class) {
	uint32_t size = sz_class_to_size(sz_class);
	uint32_t idx = (uint32_t)
		((((ptr_int_t) ptr) - ((ptr_int_t) s) -
		  PACKED_SLAB_HEADER_SIZE) / size);

	uint16_t alloc_cnt = s->alloc_cnt - 1;
	uint64_t bitv = s->pl_f;
	if (bitv == 0) {
		small_bin_link(h, s, size);
	}
	bitv |= (1LU << idx);
	s->alloc_cnt = alloc_cnt;
	s->pl_f = bitv;
	if (alloc_cnt == 0) {
		small_bin_unlink(s);
		__int_free_slab(h, s, 1);
	}
}


/*
 * special case of tiny_bitv_index for 16-sized slabs
 */
static inline uint8_t tiny_bitv_index_16(slab_t * s, void * ptr) {
	uint32_t offset = ((ptr_int_t) ptr) - ((ptr_int_t) s);
	return ((offset - PACKED_SLAB_16_HEADER_SIZE) / 16);
}

/*
 * inverse of above, gives a pointer to the n'th 16-byte block of a 16-byte
 * size class packed slab
 */
static inline void * tiny_block_ptr_16(slab_t * s, uint8_t idx) {
	return (void *)
		((((ptr_int_t) s) + PACKED_SLAB_16_HEADER_SIZE) + (idx * 16));
}

/*
 * special case of tiny_bitv_index for 32-sized slabs
 */
static inline uint8_t tiny_bitv_index_32(slab_t * s, void * ptr) {
	uint32_t offset = ((ptr_int_t) ptr) - ((ptr_int_t) s);
	return ((offset - PACKED_SLAB_32_HEADER_SIZE) / 32);
}

/*
 * inverse of above, gives a pointer to the n'th 32-byte block of a 32-byte
 * size class packed slab
 */
static inline void * tiny_block_ptr_32(slab_t * s, uint8_t idx) {
	return (void *)
		((((ptr_int_t) s) + PACKED_SLAB_32_HEADER_SIZE) + (idx * 32));
}

/*
 * special case of tiny_bitv_index for 48-sized slabs
 */
static inline uint8_t tiny_bitv_index_48(slab_t * s, void * ptr) {
	uint32_t offset = ((ptr_int_t) ptr) - ((ptr_int_t) s);
	return ((offset - PACKED_SLAB_48_HEADER_SIZE) / 48);
}

/*
 * inverse of above, gives a pointer to the n'th 48-byte block of a 48-byte
 * size class packed slab
 */
static inline void * tiny_block_ptr_48(slab_t * s, uint8_t idx) {
	return (void *)
		((((ptr_int_t) s) + PACKED_SLAB_48_HEADER_SIZE) + (idx * 48));
}

/*
 * returns the index of a tiny block within a packed slab. This can be done by
 * subtracting the offset of the first elment and dividing by the size, but we
 * define the offset of the first element to be immediately after the header,
 * which is only 32 bytes, so just dividing by size is sufficient
 */
static inline uint8_t tiny_bitv_index(slab_t * s, void * ptr) {
	uint32_t offset = ((ptr_int_t) ptr) - ((ptr_int_t) s);
	return offset / packed_slab_block_size(s);
}

/*
 * inverse of tiny_bitv_index, given a slab, its true size class, and index of
 * the block you want, returns a pointer to that block
 */
static inline void * tiny_block_ptr(slab_t * s, uint32_t size, uint8_t idx) {
	return (void *) (((ptr_int_t) s) + PACKED_SLAB_HEADER_SIZE + (idx * size));
}


/*
 * returns a pointer to the root of the list, which should be the prev pointer
 * of the first packed slab in the freelist
 */
static inline slab_t * __small_bin_start(slab_t ** small_bin) {
	return (slab_t *) (((ptr_int_t) small_bin) - offsetof(slab_t, next_p));
}


/*
 * inserts the slab into the given slab bin
 */
static inline void __small_bin_link(slab_t ** small_bin, slab_t * s) {
	// set prev to point to the right address so that when the "next" field
	// of the slab is accessed, you get the bin itself
	slab_t * next_p = *small_bin;
	slab_t * prev_p = __small_bin_start(small_bin);

	s->prev_p = prev_p;
	s->next_p = next_p;
	*small_bin = s;
	if (next_p != NULL) {
		next_p->prev_p = s;
	}
}

/*
 * inserts the slab into the correct slab bin, where size must match the
 * num_slabs field of s
 */
static inline void small_bin_link(heap_t * h, slab_t * s, uint32_t size) {
	uint8_t bin_idx = packed_bin_idx(size);
	__small_bin_link(&h->smallbins[bin_idx], s);
}

/*
 * removes the slab from its freelist
 */
static inline void small_bin_unlink(slab_t * s) {
	slab_t * next_p = s->next_p;
	slab_t * prev_p = s->prev_p;
	prev_p->next_p = next_p;
	if (next_p != NULL) {
		next_p->prev_p = prev_p;
	}
}



/*
 * --------------------- medium block helper functions ---------------------
 */

// forward declarations
static inline void medium_bin_link(heap_t * h, block_t * b, size_t size);
static inline void medium_bin_unlink(block_t * b);


static inline uint8_t is_medium_block_size(size_t size) {
	return size > MAX_TINY_BLOCK_SZ && size <= MAX_MEDIUM_BLOCK_SZ;
}

// to be used when addressing, so no conversion from uint32_t -> uint64_t is
// necessary
static inline uint64_t medium_bin_idx(size_t size) {
	return (size - MIN_MEDIUM_BLOCK_SZ) / MALLOC_ALIGN;
}

static inline block_t ** get_medium_bin(heap_t * h, size_t size) {
	return &h->mediumbins[medium_bin_idx(size)];
}

// inverse of medium_bin_index, gives the size of blocks in the bin at the
// given bin index
static inline size_t medium_bin_idx_size(uint64_t bin_idx) {
	return (bin_idx * MALLOC_ALIGN) + MIN_MEDIUM_BLOCK_SZ;
}

/*
 * in the following two methods, we are safe to "downgrade" bin_idx to a 32-bit
 * integer since we know it will never get that large, and won't require zeroing
 * out the upper half of its register
 */
static inline uint8_t medium_bin_skiplist_idx(uint32_t bin_idx) {
	return bin_idx / (8 * sizeof(uint32_t));
}

static inline uint8_t medium_bin_skiplist_bit(uint32_t bin_idx) {
	return bin_idx & ((8 * sizeof(uint32_t)) - 1);
}


/*
 * returns the offsets vector with the implied 0x02 (32-bytes) placed at the
 * beginning
 */
static inline uint64_t __medium_bin_offsets(slab_t * s) {
	uint64_t mem;
	// mask off the flags bits from the slab struct an insert the implied
	// offset of 32 (i.e. 2) at the beginning
	__asm__("mov (%[slab]),%[mem]\n\t"
			"movb %b[off0],%b[mem]\n"
			: [mem]  "=r" (mem)
			: [slab] "r"  (s),
			  [off0] "i"  (LARGE_SLAB_HEADER_SIZE / MALLOC_ALIGN));
	return mem;
}

/*
 * converts the first 8 bytes of the slab (*((uint64_t *) s)) to the offsets
 * list, similar to __medium_bin_offsets above
 */
static inline uint64_t __medium_bin_mem_to_offsets(uint64_t mem) {
	mem = (LARGE_SLAB_HEADER_SIZE / MALLOC_ALIGN) | (0xffffffffffffff00LU & mem);
	return mem;
}

static inline uint64_t __medium_bin_get_offset(uint64_t block_offs,
		uint32_t idx) {
	return ((block_offs >> (idx * 8)) & 0xff) * MALLOC_ALIGN;
}

// same as __medium_bin_get_offset, but gives SLAB_SIZE instead of 0 for 0
// entries
static inline uint32_t __medium_bin_get_adj_offset(uint64_t block_offs,
		uint32_t idx) {
	uint64_t off = __medium_bin_get_offset(block_offs, idx);
	return idx == 8 ? SLAB_SIZE : (off | ((!off) << SLAB_SIZE_SHIFT));
}


/*
 * returns the size of the block at the specified index, with idx = 0
 * corresponding to the first block
 */
static inline uint64_t medium_bin_block_size(slab_t * s, uint8_t idx) {
	uint64_t block_offs = __medium_bin_offsets(s);
	uint64_t block_sz;
	uint64_t offset = __medium_bin_get_offset(block_offs, idx);
	uint64_t next_off =
		(idx == 7 ? 0 : __medium_bin_get_offset(block_offs, idx + 1));

	if (next_off == 0) {
		// if offset is 0, make it SLAB_SIZE, otherwise leave it alone
		offset |= (!offset) << SLAB_SIZE_SHIFT;

		if (idx != 7 && (s->block_alloc & (2 << idx))) {
			// if this is not the last block (which must necessarily go to the
			// end of the slab), and the next slab is alloc'ed, then it must be
			// a large block that's a multiple of SLAB_SIZE
			block_sz = SLAB_SIZE - offset;
		}
		else {
			// continues to the end
			block_sz = large_slab_get_size(s) * SLAB_SIZE - offset;
		}
	}
	else {
		block_sz = next_off - offset;
	}
	return block_sz;
}

/*
 * finds the index in the block_offs vector this block is at, assuming b
 * is actually a part of this list
 */
static inline uint32_t medium_bin_find_block_pos(slab_t * s, block_t * b) {
	uint8_t offset = (((ptr_int_t) b) - ((ptr_int_t) s)) / MALLOC_ALIGN;
	// since we want to ignore the first byte, shift it off.
	// this also covers the case where the block is on a SLAB_SIZE boundary, as
	// offset == 0, so 0's will be shifted in and 8 will be returned if the
	// rest of the offsets list is full
	uint64_t mem = __medium_bin_offsets(s);

	// search for offset in block_offs
	__m64 broadcast  = _mm_set1_pi8(offset);
	__m64 block_offs = _m_from_int64((int64_t) mem);
	__m64 search     = _mm_cmpeq_pi8(block_offs, broadcast);;
	uint64_t si      = _mm_cvtm64_si64(search);
	MALLOC_ASSERT(si != 0);
	return __builtin_ctzl(si) >> 3;
}


/*
 * inserts the offset into the offsets list, being placed at index
 * (after_idx+1) and pushing the offsets after after_idx down by one
 *
 * note: this does not update the alloc bitvector
 */
 void __medium_bin_push_offset(slab_t * s, uint8_t after_idx,
		uint64_t offset) {
	MALLOC_ASSUME(after_idx < 7);

	uint8_t shift_amt = after_idx * 8;
	uint64_t mem;
	// the compiler wants to reference mem twice in the andq ops, but we force
	// it to use a register so memory is only accessed once (+1 instruction is
	// the cost)
	__asm__("mov %[s_start],%[mem]\n"
			: [mem] "=r" (mem)
			: [s_start] "m" (*((uint64_t *) s)));
	uint64_t keep_mask = (0x100LU << shift_amt) - 1;
	uint64_t move_mask = ~keep_mask;
	uint64_t new_mem = (mem & keep_mask) |
		((offset << (8 - MALLOC_ALIGN_SHIFT)) << shift_amt) |
		((mem & move_mask) << 8);
	*((__uint64_t *) s) = new_mem;
}

/*
 * the same as __medium_bin_push_offset, but it assumes that after_idx is the
 * index of the last offset in the list
 */
static inline void __medium_bin_append_offset(slab_t * s, uint8_t after_idx,
		uint64_t offset) {
	MALLOC_ASSUME(after_idx < 7);

	uint8_t shift_amt = after_idx * 8;
	uint64_t mem = *((__uint64_t *) s);
	uint64_t keep_mask = (0x100LU << shift_amt) - 1;
	mem = (mem & keep_mask) |
		((offset << (8 - MALLOC_ALIGN_SHIFT)) << shift_amt);
	*((__uint64_t *) s) = mem;
}

/*
 * splits the block at after_idx at the point "offset", and sets the alloc
 * bit of offset to 0
 *
 * TODO can probably move 1 or 2 ops from assembly, but this is rarely called
 */
static inline void __medium_bin_split_block(slab_t * s, uint8_t after_idx,
		uint64_t offset) {

	__medium_bin_push_offset(s, after_idx, offset);

	// push flags down, while setting the alloc bit for this split
	// block
	uint8_t block_alloc = s->block_alloc;
	uint8_t keep_mask = (2 << after_idx) - 1;
	block_alloc = (block_alloc & keep_mask) |
		((block_alloc & ~keep_mask) << 1);
	s->block_alloc = block_alloc;
}

/*
 * removes the offset after idx from the offset list
 * this effectively extends the block at after_idx into the block at
 * after_idx + 1
 */
static inline void __medium_bin_remove_offset(slab_t * s, uint8_t after_idx) {
	MALLOC_ASSUME(after_idx < 7);

	// shift offsets
	uint8_t shift_amt = after_idx * 8;
	uint64_t mem = *((__uint64_t *) s);
	uint64_t keep_mask = (0x100LU << shift_amt) - 1;
	uint64_t move_mask = ~keep_mask;
	mem = (mem & keep_mask) | ((mem >> 8) & move_mask);
	*((__uint64_t *) s) = mem;

	// shift alloc bits
	uint8_t block_alloc = s->block_alloc;
	uint8_t bkeep_mask = (2 << after_idx) - 1;
	block_alloc = (block_alloc & bkeep_mask) |
		((block_alloc >> 1) & ~bkeep_mask);
	s->block_alloc = block_alloc;
}

/*
 * frees the block at the given index
 */
static inline void medium_bin_free(heap_t * h, slab_t * s, uint8_t idx) {
	uint64_t mem = *((uint64_t *) s);
	uint64_t keep_mask = (0x100LU << (8 * idx)) - 1;
	uint64_t move_mask = ~keep_mask;
	uint8_t  alloc_keep_mask = (0x2 << idx) - 1;
	uint8_t  alloc_move_mask = ~alloc_keep_mask;
	// we will always clear the bit for the block at idx, so just remove it
	// from the keep mask
	alloc_keep_mask >>= 1;

	// coalesce with blocks around
	uint8_t shift_amt = 0;
	// mark the block before the first and the block after the last as
	// allocated to prevent coalescing with them
	uint8_t block_alloc = s->block_alloc;
	uint16_t alloc_bits = (((uint16_t) block_alloc) << 1) | 0x201;
	// if the previous block was free, merge with it
	uint8_t is_prev_free = !(alloc_bits & (1u << idx));
	// shift by 8 if previous is free
	keep_mask >>= (is_prev_free << 3);
	alloc_keep_mask >>= is_prev_free;
	shift_amt += is_prev_free;

	// no matter which case we take, the alloc bit must not be set if the next
	// adjacent block is free, so we can check that condition before the branch
	uint8_t is_next_free = !(alloc_bits & (4u << idx));
	uint64_t n_slabs = large_slab_get_size(s);
	if (n_slabs == 1) {
		// since there can't be any large chunks hanging over the end of the
		// slab, we know the next block is free only if its bit is not set in
		// alloc_bits and its offset is not 0
		// note: potential shift by 64 is safe because is_next_free will
		// already be false in this case
		is_next_free = is_next_free && ((mem >> ((idx + 1) * 8)) & 0xff) != 0;
	}
	else {
		// check if this is a large block being freed, which is the case if its
		// offset is 0 or if the next offset is 0 and marked as allocated
		uint64_t offsets_after = mem >> (8 * idx);
		if (!offsets_after || ((offsets_after >> 8) == 0 &&
					!(alloc_bits & (4u << idx)))) {
			// if the block we are freeing is a large block hanging over the edge,
			// then free the remaining slabs
			large_slab_set_size(s, 1);
			__int_free_remainder_slab(h, s + 1, n_slabs - 1);
			(s + 1)->flags |= SLAB_PREV_ALLOC_BIT;
			is_next_free = 0;
			n_slabs = 1;
		}
		else {
			// if this is not a large block being freed, we can do the same
			// check as the n_slabs = 1 case, since if the next block is large,
			// it will be marked as allocated in alloc_bits
			is_next_free = is_next_free && ((mem >> ((idx + 1) * 8)) & 0xff) != 0;
		}
	}

	// if the next is free, then we will delete its offset
	move_mask <<= (is_next_free << 3);
	alloc_move_mask <<= is_next_free;
	shift_amt += is_next_free;

	uint64_t offs = __medium_bin_mem_to_offsets(mem);

	// to be the beginning offset and end offset of the new merged block
	uint64_t start_offset;
	uint64_t end_offset;
	// remove the medium blocks from their respective bins if they were
	// coalesced
	if (is_prev_free) {
		// if previous was free, then the offset of the previous is the new
		// start offset
		start_offset = ((offs >> (8 * idx - 8)) & 0xff) * MALLOC_ALIGN;
		// we also need to remove the previous from its bin
		medium_bin_unlink((block_t *) (((ptr_int_t) s) + start_offset));
	}
	else {
		// otherwise the start offset is just the offset of this block
		start_offset = ((offs >> (8 * idx)) & 0xff) * MALLOC_ALIGN;
	}
	if (is_next_free) {
		// take the offset of the next block and remove it from its bin
		uint64_t next_offset = ((offs >> (8 * idx) >> 8) & 0xff) * MALLOC_ALIGN;
		medium_bin_unlink((block_t *) (((ptr_int_t) s) + next_offset));

		// if the next was free, then we know it wasn't a large block, so we
		// can find the end of it by skipping over and finding the offset of
		// the next block (which is guaranteed to be well-defined)
		end_offset = (((offs >> (8 * idx)) >> 16) & 0xff) * MALLOC_ALIGN;
	}
	else {
		// otherwise the end offset is just the offset of the next adjacent block
		end_offset = (((offs >> (8 * idx)) >> 8) & 0xff) * MALLOC_ALIGN;
	}
	// if start_offset or end_offset was 0x00, then treat this as an offset of
	// SLAB_SIZE
	start_offset |= (!start_offset) << SLAB_SIZE_SHIFT;
	end_offset   |= (!end_offset) << SLAB_SIZE_SHIFT;

	uint64_t new_size = end_offset - start_offset;

	uint8_t insert_free_block = 1;
	if (new_size < MIN_MEDIUM_BLOCK_SZ) {
		// if the remainder is too small (can happen if a large block is freed
		// and the remainder it leaves doesn't coalesce), then simple merge it
		// with the last block, whether or not it is allocated
		// we can ignore move_mask's since this only happens in the case
		// mentioned, meaning only zeros follow
		// we don't modify alloc_keep_mask since it is already offset by 1 to
		// account for this block being freed
		MALLOC_ASSERT(idx > 0);
		start_offset = ((offs >> (8 * idx - 8)) & 0xff) * MALLOC_ALIGN;
		keep_mask >>= 8;

		if (!((block_alloc >> (idx - 1)) & 1)) {
			medium_bin_unlink((block_t *) (((ptr_int_t) s) + start_offset));
		}
		else {
			// if this block was allocated, don't try inserting it in a free list
			insert_free_block = 0;
		}
	}
	mem = (mem & keep_mask) | ((mem & move_mask) >> (8 * shift_amt));
	block_alloc = (block_alloc & alloc_keep_mask) |
		((block_alloc & alloc_move_mask) >> shift_amt);

	if (block_alloc == 0) {
		// this slab is now totally empty, go ahead and free it
		__int_free_slab(h, s, n_slabs);
	}
	else {
		if (insert_free_block) {
			// put the new free block in the correct bin
			medium_bin_link(h, (block_t *) (((ptr_int_t) s) + start_offset),
					end_offset - start_offset);
		}

		// write back to the slab
		*((uint64_t *) s) = mem;
		s->block_alloc = block_alloc;
	}
}


static inline block_t * medium_bin_start(block_t ** bin) {
	return (block_t *) (((ptr_int_t) bin) - offsetof(block_t, next));
}


/*
 * inserts the medium sized block into the freelist and updates the skiplist
 */
static inline void medium_bin_link(heap_t * h, block_t * b, size_t size) {
	block_t ** list_head = get_medium_bin(h, size);
	block_t * second = *list_head;

	b->next = second;
	b->prev = medium_bin_start(list_head);
	if (second != NULL) {
		second->prev = b;
	}
	*list_head = b;

	// update skiplist
	uint32_t bin_idx = (uint32_t) ((size - MIN_MEDIUM_BLOCK_SZ) / MALLOC_ALIGN);
	uint8_t skip_idx = medium_bin_skiplist_idx(bin_idx);
	uint8_t bit_idx  = medium_bin_skiplist_bit(bin_idx);
	h->med_skiplist[skip_idx] |= (1U << bit_idx);
}

static inline void medium_bin_unlink(block_t * b) {
	block_t * next = b->next;
	block_t * prev = b->prev;
	prev->next = next;
	if (next != NULL) {
		next->prev = prev;
	}
}




/*
 * --------------------- heap helper functions ---------------------
 */


/*
 * returns a pointer to the first slab in the heap
 */
static slab_t * heap_start(heap_t * h) {
	return (slab_t *) (((uint64_t) h) + ALIGN_UP(sizeof(heap_t), SLAB_SIZE));
}

/*
 * returns a pointer just past the end of the last slab owned by malloc so far
 * (i.e. that has been sbrked)
 */
static slab_t * heap_end(heap_t * h) {
	return h->heap_end;
}







/* Function prototypes for internal helper routines */
bool checkheap(int lineno);
static slab_t *extend_heap(size_t size);

#ifdef DEFINE_CHECKS
static void print_heap();
#endif


bool mm_init(void) 
{
	heap_t * h;
	h = mem_sbrk(ALIGN_UP(sizeof(heap_t), SLAB_SIZE));

	for (uint32_t i = 0; i < NUM_SLAB_BINS; i++) {
		slab_bin_t * slab_bin = &h->slab_bins[i];
		slab_t * start = __slab_bin_start(slab_bin);
		slab_bin->head = start;
		slab_bin->tail = start;
	}
	__builtin_memset(&h->smallbins, 0, offsetof(heap_t, heap_end) -
			offsetof(heap_t, smallbins));

	h->heap_end = heap_start(h);
	// tread the heap struct memory as allocated memory so we don't try reading
	// from it in alloc_slab
	h->flags = LAST_SLAB_ALLOC;

	heap = h;

	return true;
}


/*
 * takes the first n_slabs slabs of the given slab, removing them from the free
 * list and potentially splitting off any remaining free slabs after
 * s + n_slabs
 *
 * note that this does not set up s in any way, it only removes the requested
 * number of slabs from it, initialization must be done by the caller
 */
static void __int_take_slab(heap_t * h, slab_t * s, uint64_t n_slabs) {
	uint64_t s_size = s->num_slabs;
	MALLOC_ASSERT(n_slabs <= s_size);

	slab_unlink(s);
	if (s_size == n_slabs) {
		// exact fit, take the whole slab

		slab_t * next_slab = s + s_size;
		if (next_slab == h->heap_end) {
			// if this is the last slab, let the heap know we're getting
			// allocated
			h->flags |= LAST_SLAB_ALLOC;
		}
		else {
			// otherwise set the prev alloc bit of the next slab
			next_slab->flags |= SLAB_PREV_ALLOC_BIT;
		}
	}
	else {
		// inexact fit, only take what is necessary

		slab_t * rem = s + n_slabs;
		// the remainder slab is free, but the prev alloc bit should be set
		rem->flags = SLAB_PREV_ALLOC_BIT;
		slab_link(h, rem, s_size - n_slabs);
		rem->num_slabs = s_size - n_slabs;
		// write footer
		*(((uint64_t *) (s + s_size)) - 1) = s_size - n_slabs;
	}
}


/*
 * allocates n_slabs contiguous slabs. This does not initialize any fields of
 * the slab
 */
static slab_t *__int_alloc_slab(heap_t * h, uint64_t n_slabs) {
	uint8_t req_bin_idx, bin_idx;
	uint64_t s_size;
	uint64_t rem;
	slab_bin_t * slab_bin;

	if (n_slabs <= MAX_SEG_SLAB_SZ) {
		// search through slab bins for a fit
		req_bin_idx = slab_bin_idx(n_slabs);

		uint8_t skip_idx = slab_bin_skiplist_idx(req_bin_idx);
		uint8_t bit_idx  = slab_bin_skiplist_bit(req_bin_idx);

		// index offset of the first bin that the current skip bitv covers
		uint32_t bin_offset = skip_idx * (8 * sizeof(uint32_t));

		uint32_t skiplist = h->slab_skiplist[skip_idx];
		// mask will cover the parts of the skiplist in memory that we want to
		// preserve when writing back
		uint32_t mask = ((1u << bit_idx) - 1);
		// save everything before this bin
		mask &= skiplist;
		// zero out everything before this bin
		skiplist ^= mask;

		for (;;) {
			if (skiplist == 0) {
				// write back the updated skiplist to memory
				h->slab_skiplist[skip_idx] = mask | skiplist;

				// we only need to keep the part of the skiplist we masked off of
				// the local variable "skiplist" in the first iteration, after now
				// we always replace the entirety of what is in memory
				mask = 0;

				skip_idx++;
				bin_offset += 8 * sizeof(uint32_t);
				if (skip_idx == SLAB_BINS_SKIPLIST_SZ) {
					break;
				}
				skiplist = h->slab_skiplist[skip_idx];
				continue;
			}

			// gives index of least significant bit in skiplist, i.e. the position
			// of the first bin which may not be empty
			bit_idx = __builtin_ctz(skiplist);

			bin_idx = bin_offset + bit_idx;
			slab_bin_t * slab_bin = &h->slab_bins[bin_idx];
			slab_t * s = slab_bin->head;
			s_size = bin_idx + 1;

			if (s != __slab_bin_start(slab_bin)) {
				// we found a slab!
				if (bin_idx == req_bin_idx) {
					// exact fit, take this slab
					slab_unlink(s);
					// set the alloc bit of s
					s->flags |= SLAB_ALLOC_BIT;

					slab_t * next_slab = s + s_size;
					if (next_slab == h->heap_end) {
						// if this is the last slab, let the heap know we're getting
						// allocated
						h->flags |= LAST_SLAB_ALLOC;
					}
					else {
						// otherwise set the prev alloc bit of the next slab
						next_slab->flags |= SLAB_PREV_ALLOC_BIT;
					}

					// we do not need to write the skiplist back, since this
					// must have been the first slab we encountered, and
					// therefore no modifications could have been made to the
					// cached skiplist

					return s;
				}
				else if (s_size > n_slabs) {
					// first, write back the updated skiplist, since slab_link
					// will also be modifying the skiplist
					h->slab_skiplist[skip_idx] = mask | skiplist;

					// inexact fit, only take what is necessary
					slab_unlink(s);
					// set the alloc bit of s
					s->flags |= SLAB_ALLOC_BIT;

					slab_t * rem = s + n_slabs;
					// the remainder slab is free, but the prev alloc bit should be set
					rem->flags = SLAB_PREV_ALLOC_BIT;
					// we can call link small, since the remainder is
					// guaranteed to be smaller than this slab which is small
					slab_link_small(h, rem, s_size - n_slabs);
					rem->num_slabs = s_size - n_slabs;
					// write footer
					*(((uint64_t *) (s + s_size)) - 1) = s_size - n_slabs;

					return s;
				}
			}
			else {
				// this list was actually empty, unset its bit in the skiplist
				skiplist &= (skiplist - 1);
			}
		}

		// now to check the large slab bin
		bin_idx = NUM_SLAB_BINS - 1;
		// for the large slab bin, we don't have to check the size. If a slab
		// is there, we will fit
		slab_bin = &h->slab_bins[bin_idx];
		slab_t * s = slab_bin->head;
		if (s != __slab_bin_start(slab_bin)) {
			s_size = s->num_slabs;
			slab_unlink(s);
			// set the alloc bit of s
			s->flags |= SLAB_ALLOC_BIT;

			slab_t * rem = s + n_slabs;
			// the remainder slab is free, but the prev alloc bit should be set
			rem->flags = SLAB_PREV_ALLOC_BIT;
			slab_link(h, rem, s_size - n_slabs);
			rem->num_slabs = s_size - n_slabs;
			// write footer
			*(((uint64_t *) (s + s_size)) - 1) = s_size - n_slabs;

			return s;
		}
	}
	else {
		// for allocations that lie in the large slab bin, we have to check the
		// size
		bin_idx = NUM_SLAB_BINS - 1;
		slab_bin_t * slab_bin = &h->slab_bins[bin_idx];
		slab_t * slab_bin_start = __slab_bin_start(slab_bin);

		// first bin may have smaller regions than we require, so we have to check
		// sizes
		for (slab_t * s = slab_bin->head; s != slab_bin_start; s = s->next) {
			// check the size of the slab region
			s_size = s->num_slabs;
			if (s_size == n_slabs) {
				// exact fit, take this slab
				slab_unlink(s);
				// set the alloc bit of s
				s->flags |= SLAB_ALLOC_BIT;

				slab_t * next_slab = s + s_size;
				if (next_slab == h->heap_end) {
					// if this is the last slab, let the heap know we're getting
					// allocated
					h->flags |= LAST_SLAB_ALLOC;
				}
				else {
					// otherwise set the prev alloc bit of the next slab
					next_slab->flags |= SLAB_PREV_ALLOC_BIT;
				}

				return s;
			}
			else if (s_size > n_slabs) {
				// inexact fit, only take what is necessary
				slab_unlink(s);
				// set the alloc bit of s
				s->flags |= SLAB_ALLOC_BIT;

				slab_t * rem = s + n_slabs;
				// the remainder slab is free, but the prev alloc bit should be set
				rem->flags = SLAB_PREV_ALLOC_BIT;
				slab_link(h, rem, s_size - n_slabs);
				rem->num_slabs = s_size - n_slabs;
				// write footer
				*(((uint64_t *) (s + s_size)) - 1) = s_size - n_slabs;

				return s;
			}
		}
	}

	// no slabs were found, so mem_sbrk more memory!
	if (!(h->flags & LAST_SLAB_ALLOC)) {
		// the last slab is free, so we only need to sbrk enough memory to
		// extend this slab to the desired length
		slab_t * last = prev_adj_slab(h->heap_end);
		slab_unlink(last);
		s_size = last->num_slabs;
		MALLOC_ASSERT(s_size < n_slabs);
		rem = n_slabs - s_size;

		slab_t * ext = mem_sbrk(rem * SLAB_SIZE);
		MALLOC_ASSERT(ext == h->heap_end);
		last->flags |= SLAB_ALLOC_BIT;

		h->heap_end += rem;
		h->flags |= LAST_SLAB_ALLOC;
		return last;
	}
	else {
		// the last slab is allocated, so we have to sbrk n_slabs slabs
		slab_t * ext = mem_sbrk(n_slabs * SLAB_SIZE);
		MALLOC_ASSERT(ext == h->heap_end);
		slab_t * last = h->heap_end;
		last->flags = SLAB_ALLOC_BIT | SLAB_PREV_ALLOC_BIT;

		h->heap_end += n_slabs;
		h->flags |= LAST_SLAB_ALLOC;
		return last;
	}
}


/*
 * initializes the slab as a packed slab of given size class, and then
 * allocates the first block in the slab and returning a pointer to it
 */
static void * __int_slab_init_packed_and_alloc(heap_t * h, slab_t * s,
		size_t size) {
	// the initial values for the f1 bitvectors of the 3 smallest block sizes
	static const uint16_t __f1_init_bitv[3] = {
		// for 16 byte blocks, we need 16 16-bit bitvectors
		0xffff,
		// for 32 byte blocks, we need 8 16-bit bitvectors
		0x00ff,
		// for 48 byte blocks, we need 6 16-bit bitvectors
		0x003f
	};

	// will point to the first allocatable block of this slab
	void * ptr;

	s->flags |= SLAB_PACKED_BIT;
	s->sz_class = size_to_sz_class(size);
	// always start with 1 block allocated
	s->alloc_cnt = 1;

	if (size <= PACKED_SLAB_2LVL_BITV_THRESH) {

		// initialize the f1 bitvector
		s->ps_f1 = __f1_init_bitv[(size / MALLOC_ALIGN) - 1];
		uint64_t * lvl2_bitv;

		// cases for initializing the f2 bitvector (with the first bit unset,
		// as the corrsponding block will be returned as an allocated block)
		switch (size) {
			case 16:
				// assuming PACKED_SLAB_16_BITV_LEN == 32
				// the first 31 bytes should all be set, and the remaining byte
				// should be set up to PACKED_SLAB_16_HEADER_SIZE/16 from the end
				// because of strict aliasing, we have to find the location of ps_f2
				// manually
				lvl2_bitv = (uint64_t *) (((ptr_int_t) s) +
						offsetof(slab_t, ps_f2));
				lvl2_bitv[0] = 0xfffffffffffffffeLU;
				lvl2_bitv[1] = 0xffffffffffffffffLU;
				lvl2_bitv[2] = 0xffffffffffffffffLU;
				lvl2_bitv[3] =
					(1LU << (64 - CEIL_DIV(PACKED_SLAB_16_HEADER_SIZE, 16))) - 1;
				ptr = tiny_block_ptr_16(s, 0);
				break;
			case 32:
				// assuming PACKED_SLAB_32_BITV_LEN == 16
				// the first 15 bytes should all be set, and the remaining byte
				// should be set up to PACKED_SLAB_16_HEADER_SIZE/16 from the end
				// because of strict aliasing, we have to find the location of ps_f2
				// manually
				lvl2_bitv = (uint64_t *) (((ptr_int_t) s) +
						offsetof(slab_t, ps_f2));
				lvl2_bitv[0] = 0xfffffffffffffffeLU;
				lvl2_bitv[1] =
					(1LU << (64 - CEIL_DIV(PACKED_SLAB_32_HEADER_SIZE, 32))) - 1;
				ptr = tiny_block_ptr_32(s, 0);
				break;
			case 48:
				// assuming PACKED_SLAB_48_BITV_LEN == 11);
				// the first 10 bytes should all be set, and the remaining byte
				// should be set up to PACKED_SLAB_16_HEADER_SIZE/16 from the end
				// because of strict aliasing, we have to find the location of ps_f2
				// manually
				lvl2_bitv = (uint64_t *) (((ptr_int_t) s) +
						offsetof(slab_t, ps_f2));
				lvl2_bitv[0] = 0xfffffffffffffffeLU;
				lvl2_bitv[1] =
					(1LU << ((SLAB_SIZE - PACKED_SLAB_48_HEADER_SIZE) / 48 - 64)) - 1;
				ptr = tiny_block_ptr_48(s, 0);
				break;
			default:
				// size must be exactly one of the above 3
				__builtin_unreachable();
		}
	}
	else {
		// for larger chunk sizes, compute bitvector initial states on the fly
		uint32_t n_elements = packed_slab_n_blocks(size);
		// by subtracting 2, the first bit will be left as 0, which we want
		// since we are allocating the first block
		s->pl_f = (1LU << n_elements) - 2;
		ptr = tiny_block_ptr(s, size, 0);
	}

	// finally, insert the slab into the corresponding free bin
	small_bin_link(h, s, size);

	return ptr;
}


/*
 * initializes the slab as a normal slab of n_slabs contiguous slabs, and
 * allocates a block of size "size" from this slab, returning a pointer to it
 */
static void * __int_slab_init_large_and_alloc(heap_t * h, slab_t * s,
		size_t n_slabs, size_t size) {

	// pointer to allocated memory to be returned
	void * ptr;

	// zero out the packed bit in case it was set
	s->flags &= ~SLAB_PACKED_BIT;

	// initialize block_offset values, which we can do with one 64-bit write
	// preserving the lowest order 8 bits (from flags)
	uint64_t block_offs = (*((uint64_t *) s)) & 0xff;
	
	// initialize the offset fields and block_alloc bitvector
	// fix the block_alloc bitvector below in each of the two cases, as the
	// block which gets allocated depends on which case is taken
	uint64_t offset_val = n_slabs;
	MALLOC_ASSERT(n_slabs < (1LU << (64 - SLAB_SIZE_SHIFT)));

	if (n_slabs == 1) {
		// medium block allocation
		MALLOC_ASSERT(size <= MAX_MEDIUM_BLOCK_SZ);

		// calculates the end of the medium block (the start of the next block,
		// which is free
		// note that if size == MAX_MEDIUM_BLOCK_SZ, this will get zeroed out,
		// indicating the end of the region
		uint64_t free_offset = size + LARGE_SLAB_HEADER_SIZE;
		uint64_t free_size = SLAB_SIZE - free_offset;
		if (free_size >= MIN_MEDIUM_BLOCK_SZ) {
			block_offs |= (((free_offset / MALLOC_ALIGN) & 0xff) << 8);
			medium_bin_link(h, (block_t *) (((ptr_int_t) s) + free_offset), free_size);
		}

		// the first block is allocated, and the second block is free if the
		// remainder was not too small, but since we will never check the
		// free/alloc bit of a block that doesn't exist in the offset list, we
		// can set it anyway
		offset_val |= (0x1LU << 56);

		// ptr is the first block
		ptr = (void *) (((ptr_int_t) s) + LARGE_SLAB_HEADER_SIZE);
	}
	else {
		MALLOC_ASSERT(size >= MIN_LARGE_BLOCK_SZ);
		// large block allocation
		// the offset of the large block to be allocated (pushed back as far as
		// possible)
		uint64_t alloc_offset = SLAB_SIZE - (size & ~SLAB_SIZE_MASK);

		uint64_t remainder_size = alloc_offset - LARGE_SLAB_HEADER_SIZE;
		if (remainder_size >= MIN_MEDIUM_BLOCK_SZ) {
			// the remainder is large enough to fit medium blocks, so insert
			// it into the proper freelist
			block_t * rem_blk =
				(block_t *) (((ptr_int_t) s) + LARGE_SLAB_HEADER_SIZE);
			medium_bin_link(h, rem_blk, remainder_size);

			// the first block is free and the second block is allocated, so mark
			// it as such
			offset_val |= (0x2LU << 56);

			block_offs |= (alloc_offset & 0xff0) << (8 - MALLOC_ALIGN_SHIFT);
		}
		else {
			// the remainder is too small for medium blocks, simply keep it
			// marked as free but don't insert into any lists, that way when
			// the large block is freed the whole slab can be freed
			// we leave the alloc_offset in block_offs to 0 to indicate that
			// this block fills the entire slab
			alloc_offset = LARGE_SLAB_HEADER_SIZE;
			offset_val |= (1LU << 56);
		}

		// ptr is at "alloc_offset" from start
		ptr = (void *) (((ptr_int_t) s) + alloc_offset);
	}

	// commit fully initialized header to memory
	*((uint64_t *) s) = block_offs;
	s->offset = offset_val;

	return ptr;
}


/*
 * frees the given slab which is assumed to be part of a larger slab (which
 * extends before s but not beyond s + n_slabs)
 *
 * same as __int_free_slab, but does not try to coalesce with slabs before this
 * one
 */
static inline void __int_free_remainder_slab(heap_t * h, slab_t * s, uint64_t n_slabs) {
	uint8_t flags = s->flags;
	slab_t * next;

	next = s + n_slabs;
	flags &= (uint8_t) (~SLAB_ALLOC_BIT);

	// if next slab is free, merge with it
	if (next == h->heap_end) {
		h->flags &= (uint8_t) (~LAST_SLAB_ALLOC);
	}
	else if (slab_is_free(next)) {
		slab_unlink(next);
		uint64_t next_n_slabs = next->num_slabs;
		n_slabs += next_n_slabs;
		next += next_n_slabs;

		// no need to look at next's next flags since next's state isn't
		// changing
	}
	else {
		next->flags &= (uint8_t) (~SLAB_PREV_ALLOC_BIT);
	}

	slab_link(h, s, n_slabs);
	s->flags = flags;
	s->num_slabs = n_slabs;

	// write footer
	*(((uint64_t *) next) - 1) = n_slabs;
}

static void __int_free_slab(heap_t * h, slab_t * s, uint64_t n_slabs) {
	uint8_t flags = s->flags;
	slab_t * next;

	next = s + n_slabs;

	// if previous slab is free, merge with it
	if (!(flags & SLAB_PREV_ALLOC_BIT)) {
		slab_t * prev = prev_adj_slab(s);

		slab_unlink(prev);
		flags = prev->flags;
		n_slabs += prev->num_slabs;
		s = prev;
	}
	else {
		flags = s->flags & (~SLAB_ALLOC_BIT);
	}

	// if next slab is free, merge with it
	if (next == h->heap_end) {
		h->flags &= ~LAST_SLAB_ALLOC;
	}
	else if (slab_is_free(next)) {
		slab_unlink(next);
		uint64_t next_n_slabs = next->num_slabs;
		n_slabs += next_n_slabs;
		next += next_n_slabs;

		// no need to look at next's next flags since next's state isn't
		// changing
	}
	else {
		next->flags &= ~SLAB_PREV_ALLOC_BIT;
	}

	slab_link(h, s, n_slabs);
	s->flags = flags;
	s->num_slabs = n_slabs;

	// write footer
	*(((uint64_t *) next) - 1) = n_slabs;
}



static void *__int_find_tiny_block(heap_t * h, size_t size) {

	MALLOC_ASSERT(size <= MAX_TINY_BLOCK_SZ);
	speak("finding tiny block for %zu\n", size);

	// only check the exact bin this block fits in
	uint8_t bin_idx = packed_bin_idx((uint32_t) size);
	speak("\tbin idx %u\n", bin_idx);
	slab_t * s = h->smallbins[bin_idx];
	if (s != NULL) {
		speak("found slab:\n");
#ifdef DO_SPEAK
		print_slab(s);
#endif
		void * ptr;
		switch (size) {
			case 16:
				ptr = __packed_alloc_16(s);
				break;
			case 32:
				ptr = __packed_alloc_32(s);
				break;
			case 48:
				ptr = __packed_alloc_48(s);
				break;
			default:
				ptr = __packed_alloc(s, size);
				break;
		}
		return ptr;
	}
	// there were no slabs for this size class with any free space
	return NULL;
}


static void *__int_find_medium_block(heap_t * h, size_t size) {
	
	// first check seglists for a fitting size
	uint64_t bin_idx = medium_bin_idx(size);

	uint8_t skip_idx = medium_bin_skiplist_idx(bin_idx);
	uint8_t bit_idx  = medium_bin_skiplist_bit(bin_idx);

	// index offset of the first bin that the current skip bitv covers
	uint32_t bin_offset = skip_idx * (8 * sizeof(uint32_t));

	uint32_t skiplist = h->med_skiplist[skip_idx];
	// mask will cover the parts of the skiplist in memory that we want to
	// preserve when writing back
	uint32_t mask = ((1u << bit_idx) - 1);
	// save everything before this bin
	mask &= skiplist;
	// zero out everything before this bin
	skiplist ^= mask;

	// TODO skip taking blocks which have a large amount of unusable space
	// (<= 512 bytes)

	for (;;) {
		if (skiplist == 0) {
			// write back the updated skiplist to memory
			h->med_skiplist[skip_idx] = mask | skiplist;

			// we only need to keep the part of the skiplist we masked off of
			// the local variable "skiplist" in the first iteration, after now
			// we always replace the entirety of what is in memory
			mask = 0;

			skip_idx++;
			bin_offset += 8 * sizeof(uint32_t);
			if (skip_idx == MEDIUMBINS_SKIPLIST_SZ) {
				break;
			}
			skiplist = h->med_skiplist[skip_idx];
			continue;
		}

		// gives index of least significant bit in skiplist, i.e. the position
		// of the first bin which may not be empty
		bit_idx = __builtin_ctz(skiplist);

		bin_idx = bin_offset + bit_idx;
		block_t ** list_head = &h->mediumbins[bin_idx];
		block_t * blk = *list_head;
		if (blk != NULL) {
			// we found a block!

			// first, unlink it from the freelist
			medium_bin_unlink(blk);
			slab_t * s = block_get_slab(blk);

			uint8_t block_pos = medium_bin_find_block_pos(s, blk);
			size_t block_sz = medium_bin_idx_size(bin_idx);
			size_t remainder = block_sz - size;
			if (remainder >= MIN_MEDIUM_BLOCK_SZ) {
				// split off the remainder of the block from this chunk, adding
				// it to its respective freelist
				block_t * rem_blk = (block_t *) (((ptr_int_t) blk) + size);
				// this will update the skiplist, even though we have one
				// 32-bit part of the skiplist cached in a register, however
				// this is ok because the remainder must be at least 32 bins
				// away from the block it came from, as size itself must be at
				// least 512 bytes
				medium_bin_link(h, rem_blk, remainder);

				// push the remainder block's offset into the list of block offsets
				__medium_bin_push_offset(s, block_pos,
						((ptr_int_t) blk) - ((ptr_int_t) s) + size);

				// push flags down, while setting the alloc bit for this split
				// block
				uint8_t block_alloc = s->block_alloc;
				uint8_t keep_mask = (1 << block_pos) - 1;
				block_alloc = (block_alloc & keep_mask) | (1 << block_pos) |
					((block_alloc & ~keep_mask) << 1);
				s->block_alloc = block_alloc;
			}
			else {
				// take the whole block, which only requires us to set the
				// alloc bit in the block_alloc field
				s->block_alloc |= (1 << block_pos);
			}


			// before returning, write back the updated skiplist to memory
			h->med_skiplist[skip_idx] = mask | skiplist;

			return blk;
		}
		else {
			// this list was actually empty! unset its bit in the skiplist
			skiplist &= (skiplist - 1);
		}
	}
	return NULL;
}


static void *__int_malloc(heap_t * h, size_t size) {
	void *ptr;
	uint64_t n_slabs;

	speak("mallocing %zu\n", size);

	if (is_tiny_block_size(size)) {
		speak("adjusting tiny size: %lu => %u\n", size, adj_tiny_size(size));
		size = adj_tiny_size(size);
		ptr = __int_find_tiny_block(h, size);
	}
	else if (is_medium_block_size(size)) {
		ptr = __int_find_medium_block(h, size);
	}
	else {
		goto alloc_slab;
	}

	if (ptr != NULL) {
		return ptr;
	}

alloc_slab:
	// either this is a large allocation, which will always require allocating
	// slabs, or we could not find a slab that fit the requested size. In
	// either case, we round up to slab size and allocate that many slabs
	n_slabs = req_slabs_for_size(size);
	slab_t * s = __int_alloc_slab(h, n_slabs);

	if (__builtin_expect(s == NULL, 0)) {
		// could not allocate the slab
		return NULL;
	}

	if (is_tiny_block_size(size)) {
		ptr = __int_slab_init_packed_and_alloc(h, s, size);
	}
	else {
		ptr = __int_slab_init_large_and_alloc(h, s, n_slabs, size);
	}

	return ptr;
}

void *malloc(size_t size) 
{
	void * ptr;
	heap_t * h = heap;
	size_t esize;

	MALLOC_ASSERT(h != NULL);
#ifdef DEBUG
	mm_checkheap(__LINE__);
#endif

	esize = ALIGN_UP(size, MALLOC_ALIGN);

	if (esize == 0) {
		return NULL;
	}

	ptr = __int_malloc(h, esize);

#ifdef DO_PRINT
	printf(P_MAGENTA BOLD "malloced %p (size = %zu => %zu)" P_DEFAULT NORMAL "\n",
			ptr, size, esize);
	print_heap(h);
#endif
#ifdef DEBUG
	mm_checkheap(__LINE__);
#endif

	return ptr;
} 


static void __int_tiny_block_free(heap_t * h, slab_t * s, void * ptr) {
	uint8_t sz_class = packed_slab_sz_class(s);

	switch (sz_class) {
		case 0:
			// 16 byte packed slab
			__packed_free_16(h, s, ptr);
			break;
		case 1:
			// 32 byte packed slab
			__packed_free_32(h, s, ptr);
			break;
		case 2:
			// 48 byte packed slab
			__packed_free_48(h, s, ptr);
			break;
		default:
			// 64+ byte packed slab
			__packed_free(h, s, ptr, sz_class);
			break;
	}
}


static void __int_large_block_free(heap_t * h, slab_t * s, void * ptr) {
	uint8_t blk_idx = medium_bin_find_block_pos(s, ptr);
	medium_bin_free(h, s, blk_idx);
}


static void __int_free(heap_t * h, void * ptr) {
	slab_t * s = ptr_get_slab(ptr);

	// make sure the pointer being freed is in an allocated slab
	MALLOC_ASSERT(slab_is_alloc(s));

	if (slab_is_packed(s)) {
		__int_tiny_block_free(h, s, ptr);
	}
	else {
		__int_large_block_free(h, s, ptr);
	}
}


void free(void * ptr)
{
	heap_t * h = heap;

	if (ptr == NULL) {
		return;
	}

	speak("freeing %p\n", ptr);

	MALLOC_ASSERT(h != NULL);
#ifdef DEBUG
	mm_checkheap(__LINE__);
#endif

	__int_free(h, ptr);

#ifdef DO_PRINT
	printf(P_MAGENTA BOLD "freed %p" P_DEFAULT NORMAL "\n", ptr);
	print_heap(h);
#endif
#ifdef DEBUG
	mm_checkheap(__LINE__);
#endif
}


static void *__int_realloc(heap_t * h, void *ptr, size_t size) {

	slab_t * s = ptr_get_slab(ptr);

	if (slab_is_packed(s)) {
		speak("is packed\n");
		uint8_t sz_class = packed_slab_sz_class(s);
		uint32_t blk_size = sz_class_to_size(sz_class);
		if (is_tiny_block_size(size) &&
				adj_tiny_size(size) == (size_t) blk_size) {
			speak("sizes are the same, keep it\n");
			// great, we can keep it!
			return ptr;
		}
		else {
			speak("sizes aren't the same, just malloc/copy/free\n");
			// find a new home
			void * new_ptr = __int_malloc(h, size);
			if (new_ptr != NULL) {
				// TODO make one specialized for 16-byte aligned chunks
				memcpy(new_ptr, ptr, MIN(size, blk_size));
			}
			switch (sz_class) {
				case 0:
					// 16 byte packed slab
					__packed_free_16(h, s, ptr);
					break;
				case 1:
					// 32 byte packed slab
					__packed_free_32(h, s, ptr);
					break;
				case 2:
					// 48 byte packed slab
					__packed_free_48(h, s, ptr);
					break;
				default:
					// 64+ byte packed slab
					__packed_free(h, s, ptr, sz_class);
					break;
			}
			return new_ptr;
		}
	}
	else {
		// either a medium or large block
		block_t * b = (block_t *) ptr;

		if (size < MIN_MEDIUM_BLOCK_SZ) {
			speak("reallocing medium size to small, have to malloc/copy/free\n");
			// don't allow blocks smaller than this in medium regions, forced
			// malloc/free
			void * new_ptr = __int_malloc(h, size);
			if (new_ptr != NULL) {
				// size is guaranteed to be smaller than blk_size, whatever it
				// is, in this case
				memcpy(new_ptr, ptr, size);
			}
			__int_large_block_free(h, s, ptr);
			return new_ptr;
		}

		uint8_t blk_idx = medium_bin_find_block_pos(s, b);
		uint64_t blk_size = medium_bin_block_size(s, blk_idx);
		if (blk_size >= MIN_LARGE_BLOCK_SZ) {
			speak("reallocing large block\n");

			if (size > blk_size) {
				speak("req size is larger, try extending\n");
				// see if there is a free slab that has enough space ahead of
				// this one that can be merged into this one

				// first have to check deadweight, don't allow extension if
				// it would waste a lot of space
				uint64_t remainder = size - blk_size;
				uint32_t deadweight = (uint32_t)
					(ALIGN_UP(remainder, SLAB_SIZE) - remainder);
				if (deadweight <= REALLOC_MAX_DEADWEIGHT) {
					// check if the next slab is free and has enough free space
					uint64_t req_n_slabs = CEIL_DIV(remainder, SLAB_SIZE);
					uint64_t s_size = large_slab_get_size(s);
					slab_t * next_slab = s + s_size;

					if (next_slab == heap_end(h)) {
						speak("slab is at the end of the heap, so extend heap "
								"%lu slabs\n", req_n_slabs);
						// this is the end of the heap, so we can just extend
						MALLOC_ASSERT(mem_sbrk(req_n_slabs * SLAB_SIZE) ==
								h->heap_end);
						h->heap_end += req_n_slabs;
						large_slab_set_size(s, s_size + req_n_slabs);
						return ptr;
					}
					else if (slab_is_free(next_slab) &&
							next_slab->num_slabs >= req_n_slabs) {
						speak("extend into next %lu slabs\n", req_n_slabs);
						// we can extend into the next slab
						large_slab_set_size(s, s_size + req_n_slabs);
						__int_take_slab(h, next_slab, req_n_slabs);
						return ptr;
					}
				}
				else {
					speak("too much deadweight\n");
				}

				speak("have to malloc/copy/free\n");
				// forced to malloc/copy/free
				void * new_ptr = __int_malloc(h, size);
				if (new_ptr != NULL) {
					// blk_size is smaller than size
					memcpy(new_ptr, ptr, blk_size);
				}
				__int_large_block_free(h, s, ptr);
				return new_ptr;
			}
			else {
				speak("req size is smaller\n");
				uint64_t remainder = blk_size - size;
				uint64_t blk_hangover = blk_size & ~SLAB_SIZE_MASK;
				// amount of the block residing in the first slab
				uint64_t blk_offset = SLAB_SIZE - blk_hangover;

				if (size <= blk_hangover) {
					speak("shrinking to a medium block of size %lu\n",
							size);
					// can shrink this down to a medium block
					// first free all slabs after this one
					__int_free_remainder_slab(h, s + 1, req_slabs_for_size(blk_size) - 1);
					(s + 1)->flags |= SLAB_PREV_ALLOC_BIT;
					large_slab_set_size(s, 1);

					// then check to see whether we should take the whole
					// remaining block or if we can split
					if (blk_hangover - size >= MIN_MEDIUM_BLOCK_SZ) {
						speak("splitting the block\n");
						// split this block
						uint64_t new_offset = blk_offset + size;
						__medium_bin_append_offset(s, blk_idx, new_offset);
						// we don't need to modify alloc_bits since the block
						// being added is free
						medium_bin_link(h,
								(block_t *) (((ptr_int_t) s) + new_offset),
								SLAB_SIZE - new_offset);
					}
					// nothing to do if we are taking the whole block
					return ptr;
				}
				// since large blocks always extend to the end of a slab, the
				// remainder is also the amount of deadwweight we have
				// however, since we can free an integer number of slabs' worth
				// of deadweight, the true deadweight is the remainder modulo
				// SLAB_SIZE
				else if (((remainder & ~SLAB_SIZE_MASK)) <= REALLOC_MAX_DEADWEIGHT) {
					speak("not too much deadweight, keep the block\n");
					uint64_t slabs_to_free = remainder / SLAB_SIZE;
					uint64_t new_n_slabs = req_slabs_for_size(size);
					MALLOC_ASSERT(new_n_slabs + slabs_to_free == large_slab_get_size(s));

					__int_free_remainder_slab(h, s + new_n_slabs, slabs_to_free);
					(s + new_n_slabs)->flags |= SLAB_PREV_ALLOC_BIT;
					large_slab_set_size(s, new_n_slabs);
					return ptr;
				}
				else if (remainder > REALLOC_MAX_DEADWEIGHT) {
					speak("req size is too much smaller, too much deadweight so "
							"malloc/copy/free\n");
					// we have to find a new place for the new block
					void * new_ptr = __int_malloc(h, size);
					if (new_ptr != NULL) {
						// size is smaller than blk_size
						memcpy(new_ptr, ptr, size);
					}
					__int_large_block_free(h, s, ptr);
					return new_ptr;
				}
				else {
					speak("not too much deadweight, keep the block\n");
					// we can keep the same block, do nothing
					return ptr;
				}
			}
		}
		else if (size <= blk_size) {
			speak("reallocing medium block\n"
					"req size is smaller or equal so shrink\n");

			// if the next block is free, merge into it
			uint64_t offsets = __medium_bin_offsets(s);
			uint32_t blk_off = __medium_bin_get_offset(offsets, blk_idx);
			uint32_t next_off = __medium_bin_get_offset(offsets, blk_idx + 1);
			uint32_t next_end;
			if (next_off != SLAB_SIZE &&
					!(((s->block_alloc >> 1) >> blk_idx) & 1)) {
				next_end = __medium_bin_get_adj_offset(offsets, blk_idx + 2);

				s->block_offs[blk_idx] = (blk_off + size) / MALLOC_ALIGN;

				block_t * next_blk = (block_t *) (((ptr_int_t) s) + next_off);
				medium_bin_unlink(next_blk);
				next_blk = (block_t *) (((ptr_int_t) s) + blk_off + size);
				medium_bin_link(h, next_blk, next_end - (blk_off + size));
				return ptr;
			}

			uint32_t remainder = blk_size - size;
			if (remainder >= MIN_MEDIUM_BLOCK_SZ) {
				speak("split into %lu and %u\n",
						size, remainder);
				// we can split the block
				uint32_t offset = (((ptr_int_t) ptr) - ((ptr_int_t) s)) + size;
				__medium_bin_split_block(s, blk_idx, offset);

				// insert the split block into the free list
				medium_bin_link(h, (block_t *) (((ptr_int_t) s) + offset),
						remainder);
				return ptr;
			}
			else {
				speak("can't split, keep the whole block\n");
				// do nothing, can simply return the same pointer
				return ptr;
			}
		}
		else if (size <= MAX_MEDIUM_BLOCK_SZ) {
			speak("reallocing medium block\n"
					"req size is larger so try to extend\n");
			// otherwise, if the requested size is larger than the block
			// itself, but isn't a large request size, see if we can extend the
			// block
			uint64_t offsets = __medium_bin_offsets(s);
			uint32_t next_off = __medium_bin_get_offset(offsets, blk_idx + 1);
			uint32_t remainder = size - blk_size;
			uint32_t next_size;

			if (next_off != 0 && !((s->block_alloc >> (blk_idx + 1)) & 1) &&
					remainder <= (next_size =
					 __medium_bin_get_adj_offset(offsets, blk_idx + 2) - next_off)) {
				speak("next block is free and has enough space\n");

				// remove the next block from the 
				block_t * next_blk = (block_t *) (((ptr_int_t) s) + next_off);
				medium_bin_unlink(next_blk);

				if (next_size - remainder < MIN_MEDIUM_BLOCK_SZ) {
					speak("take the whole thing\n");
					// take the whole block, so just need to remove the next
					// block from the offset list
					__medium_bin_remove_offset(s, blk_idx);
					return ptr;
				}
				else {
					speak("split the next block, now size %u\n", next_size - remainder);
					// extend this block into the next
					next_off += remainder;
					// write this new offset to the offset list, we just
					// requires direct indexing into block_offs with blk_idx,
					// as this list starts from the second offset
					s->block_offs[blk_idx] = next_off / MALLOC_ALIGN;
					// no need to update block_alloc, since the relative
					// positions of the blocks haven't changed
					medium_bin_link(h, (block_t *) (((ptr_int_t) next_blk) + remainder),
							next_size - remainder);
					return ptr;
				}
			}
			speak("not enough space, have to malloc/copy/free\n");
		}
		// we're forced to malloc/copy/free, either because the requested size
		// was a large block size or we couldn't merge into the next block (it
		// either wasn't free or it wasn't big enough)
		void * new_ptr = __int_malloc(h, size);
		if (new_ptr != NULL) {
			// blk_size is smaller than size
			memcpy(new_ptr, ptr, blk_size);
		}
		__int_large_block_free(h, s, ptr);
		return new_ptr;
	}
}


void *realloc(void *ptr, size_t size)
{
	heap_t * h = heap;
	void * new_ptr;
	size_t esize;

	speak("reallocing %p => %zu\n", ptr, size);

	MALLOC_ASSERT(h != NULL);
#ifdef DEBUG
	mm_checkheap(__LINE__);
#endif

	esize = ALIGN_UP(size, MALLOC_ALIGN);

	if (ptr == NULL) {
		if (esize == 0) {
			return NULL;
		}
		else {
			return __int_malloc(h, esize);
		}
	}
	else if (esize == 0) {
		__int_free(h, ptr);
		return NULL;
	}

	new_ptr = __int_realloc(h, ptr, esize);

#ifdef DO_PRINT
	printf(P_MAGENTA BOLD "realloced %p => %p" P_DEFAULT NORMAL "\n", ptr,
			new_ptr);
	print_heap(h);
#endif
#ifdef DEBUG
	mm_checkheap(__LINE__);
#endif

    return new_ptr;
}

void *calloc(size_t nmemb, size_t size)
{
	void * ptr;
	heap_t * h = heap;
	size_t esize;

	MALLOC_ASSERT(h != NULL);
#ifdef DEBUG
	mm_checkheap(__LINE__);
#endif

	size = nmemb * size;
	esize = ALIGN_UP(size, MALLOC_ALIGN);

	if (esize == 0) {
		return NULL;
	}

	ptr = __int_malloc(h, esize);

	memset(ptr, 0, size);

#ifdef DO_PRINT
	printf(P_MAGENTA BOLD "calloced %p (size = %zu => %zu)" P_DEFAULT NORMAL "\n",
			ptr, size, esize);
	print_heap(h);
#endif
#ifdef DEBUG
	mm_checkheap(__LINE__);
#endif

	return ptr;
}


#ifndef DEFINE_CHECKS

bool mm_checkheap(int lineno) {
	(void) lineno;
	return 1;
}

#else

bool mm_checkheap(int lineno)
{
	heap_t * h = heap;
	uint32_t free_slab_cnts[NUM_SLAB_BINS]  = { 0 };
	uint32_t smallbin_cnts[NUM_SMALLBINS]   = { 0 };
	uint32_t mediumbin_cnts[NUM_MEDIUMBINS] = { 0 };

	(void) lineno;

	int last_slab_is_alloc = 1;

	// validate free slabs
	// iterate through the whole heap and count the free slabs
	for (slab_t * s = heap_start(h); s != heap_end(h); s = next_adj_slab(s)) {
		if (slab_is_free(s)) {
			MALLOC_ASSERT(slab_get_size(s) > 0);
			free_slab_cnts[slab_bin_idx(slab_get_size(s))]++;
			last_slab_is_alloc = 0;
			// validate footer
			uint64_t * footer = ((uint64_t *) (s + s->num_slabs)) - 1;
			MALLOC_ASSERT(s->num_slabs == *footer);

			// no adjacent free slabs allowed
			MALLOC_ASSERT(s->flags & SLAB_PREV_ALLOC_BIT);

			slab_t * next_adj = next_adj_slab(s);
			MALLOC_ASSERT(next_adj == h->heap_end || !(next_adj->flags & SLAB_PREV_ALLOC_BIT));
		}
		else if (slab_is_packed(s)) {
			uint32_t block_sz = packed_slab_block_size(s);
			int has_free_space;

			slab_t * next_adj = next_adj_slab(s);
			MALLOC_ASSERT(next_adj == h->heap_end || (next_adj->flags & SLAB_PREV_ALLOC_BIT));

			// a count of the number of allocated blocks
			uint32_t cnt = 0;

			switch (block_sz) {
				// iterate through bitvector and make sure level 1 is only
				// unset if level 2 is 0
				case 16:
					for (int i = 0; i < CEIL_DIV(PACKED_SLAB_16_BITV_LEN, 2); i++) {
						MALLOC_ASSERT(((s->ps_f1 >> i) & 1) ^ (!s->ps_f2[i]));
					}
					for (int i = 0; i < CEIL_DIV(PACKED_SLAB_16_BITV_LEN, 8); i++) {
						uint64_t bitv = ((uint64_t *) s->ps_f2)[i];
						cnt += __builtin_popcountl(bitv);
					}
					cnt = PACKED_SLAB_16_BITV_BITS - cnt;
					has_free_space = (s->ps_f1 != 0);
					break;
				case 32:
					for (int i = 0; i < CEIL_DIV(PACKED_SLAB_32_BITV_LEN, 2); i++) {
						MALLOC_ASSERT(((s->ps_f1 >> i) & 1) ^ (!s->ps_f2[i]));
					}
					for (int i = 0; i < CEIL_DIV(PACKED_SLAB_32_BITV_LEN, 8); i++) {
						uint64_t bitv = ((uint64_t *) s->ps_f2)[i];
						cnt += __builtin_popcountl(bitv);
					}
					cnt = PACKED_SLAB_32_BITV_BITS - cnt;
					has_free_space = (s->ps_f1 != 0);
					break;
				case 48:
					for (int i = 0; i < CEIL_DIV(PACKED_SLAB_48_BITV_LEN, 2); i++) {
						MALLOC_ASSERT(((s->ps_f1 >> i) & 1) ^ (!s->ps_f2[i]));
					}
					for (int i = 0; i < CEIL_DIV(PACKED_SLAB_48_BITV_LEN, 8); i++) {
						uint64_t bitv = ((uint64_t *) s->ps_f2)[i];
						cnt += __builtin_popcountl(bitv);
					}
					cnt = PACKED_SLAB_48_BITV_BITS - cnt;
					has_free_space = (s->ps_f1 != 0);
					
					// make sure the bitvector is 0 for every bit past the end
					MALLOC_ASSERT((((uint64_t *) s->ps_f2)[1] &
								~((1LU <<
										((SLAB_SIZE - PACKED_SLAB_48_HEADER_SIZE) / 48
										 - 64)) - 1)) == 0);
					break;
				default: {
						// make sure the bitvector is 0 for every bit past the
						// end
						uint64_t mask =
							~((1LU << packed_slab_n_blocks(block_sz)) - 1);
						MALLOC_ASSERT((s->pl_f & mask) == 0);

						cnt = __builtin_popcountl(s->pl_f);
						cnt = packed_slab_n_blocks(block_sz) - cnt;

						has_free_space = (s->pl_f != 0);
						break;
					}
			}

			MALLOC_ASSERT(cnt == s->alloc_cnt);

			if (has_free_space) {
				uint32_t bin_idx = packed_bin_idx(block_sz);
				smallbin_cnts[bin_idx]++;
			}
			last_slab_is_alloc = 1;
		}
		else {
			slab_t * next_adj = next_adj_slab(s);
			MALLOC_ASSERT(next_adj == h->heap_end || (next_adj->flags & SLAB_PREV_ALLOC_BIT));

			uint8_t adj_frees = ~(s->block_alloc | (s->block_alloc >> 1));

			// large slab
			// validate block_alloc and block_offs, the first 0 byte of
			// block_offs indicates the end of the block
			for (uint32_t idx = 0; idx < 8; idx++) {
				if (idx == 7 || s->block_offs[idx] == 0) {
					// if this slab is size 1, then there can't be a large slab
					// extending beyond the end of the slab
					MALLOC_ASSERT(large_slab_get_size(s) > 1 || idx == 7 ||
							!((s->block_alloc >> (idx + 1)) & 1));
					// if this slab is over size 1, the last nonzero-offset
					// block can't be free unless there is an allocated block a
					// multiple of SLAB_SIZE
					MALLOC_ASSERT(large_slab_get_size(s) == 1 ||
							(((s->block_alloc | (s->block_alloc >> 1)) >> idx) & 1));

					if (!((s->block_alloc >> idx) & 1)) {
						uint32_t offset = idx == 0 ? LARGE_SLAB_HEADER_SIZE :
							s->block_offs[idx - 1] * MALLOC_ALIGN;
						if (large_slab_get_size(s) == 1 || s->block_alloc & (2 << idx)) {
							uint32_t block_sz = SLAB_SIZE - offset;
							uint32_t med_bin_idx = medium_bin_idx(block_sz);
							mediumbin_cnts[med_bin_idx]++;
						}
						else {
							// continues to the end
							// the rest of the alloc bits must be clear after this point
							MALLOC_ASSERT(!(s->block_alloc & ((uint8_t) (~((2 << idx) - 1)))));
						}
					}

					// verify that the rest of the slots are also 0
					for (idx++; idx < 7; idx++) {
						MALLOC_ASSERT(s->block_offs[idx] == 0);
					}
					break;
				}
				else {
					uint32_t offset = idx == 0 ? LARGE_SLAB_HEADER_SIZE :
						s->block_offs[idx - 1] * MALLOC_ALIGN;
					uint32_t next_off = s->block_offs[idx] * MALLOC_ALIGN;
					MALLOC_ASSERT(next_off > offset);

					MALLOC_ASSERT(!((adj_frees >> idx) & 1));

					if (!((s->block_alloc >> idx) & 1)) {
						uint32_t med_bin_idx = medium_bin_idx(next_off - offset);
						mediumbin_cnts[med_bin_idx]++;
					}
				}
			}
			last_slab_is_alloc = 1;
		}
	}

	MALLOC_ASSERT(last_slab_is_alloc == ((h->flags & LAST_SLAB_ALLOC) != 0));

	// iterate through the slab bins and make sure their counts are correct
	for (uint32_t bin_idx = 0; bin_idx < NUM_SLAB_BINS; bin_idx++) {
		uint32_t cnt = 0;

		slab_bin_t * slab_bin = &h->slab_bins[bin_idx];
		slab_t * slab_bin_start = __slab_bin_start(&h->slab_bins[bin_idx]);
		slab_t * prev_slab = slab_bin_start;
		for (slab_t * s = slab_bin->head; s != slab_bin_start; s = s->next) {
			cnt++;
			MALLOC_ASSERT(s->prev == prev_slab);
			MALLOC_ASSERT(slab_is_free(s));
			MALLOC_ASSERT(slab_bin_idx(s->num_slabs) == bin_idx);

			if (bin_idx == NUM_SLAB_BINS - 1) {
				// verify that it's sorted
				MALLOC_ASSERT(prev_slab == slab_bin_start ||
						(prev_slab->num_slabs <= s->num_slabs));
			}

			// don't expect more than 1000000 slabs in any bin, likely a loop
			MALLOC_ASSERT(cnt < 1000000);
			prev_slab = s;
		}

		MALLOC_ASSERT(free_slab_cnts[bin_idx] == cnt);

		uint8_t s_idx = slab_bin_skiplist_idx(bin_idx);
		uint8_t s_bit = slab_bin_skiplist_bit(bin_idx);
		if (bin_idx == NUM_SLAB_BINS - 1) {
			// never mark the last slab bin in the skiplist
			MALLOC_ASSERT(!((h->slab_skiplist[s_idx] >> s_bit) & 1));
		}
		else {
			MALLOC_ASSERT(cnt == 0 || ((h->slab_skiplist[s_idx] >> s_bit) & 1));
		}
	}

	// iterate through the small bins, verifying that they only contain packed
	// slabs of the correct size and that the number of slabs in them are
	// correct
	for (uint32_t bin_idx = 0; bin_idx < NUM_SMALLBINS; bin_idx++) {
		uint32_t cnt = 0;
		slab_t * prev_slab = __small_bin_start(&h->smallbins[bin_idx]);
		for (slab_t * s = h->smallbins[bin_idx]; s != NULL; s = s->next_p) {
			cnt++;
			MALLOC_ASSERT(s->prev_p == prev_slab);
			MALLOC_ASSERT(slab_is_packed(s));
			MALLOC_ASSERT(packed_bin_idx(packed_slab_block_size(s)) == bin_idx);

			uint32_t size = sz_class_to_size(s->sz_class);
			switch (size) {
				case 16:
					MALLOC_ASSERT(!packed_slab_is_empty(s));
					MALLOC_ASSERT(!packed_slab_16_is_full(s));
					break;
				case 32:
					MALLOC_ASSERT(!packed_slab_is_empty(s));
					MALLOC_ASSERT(!packed_slab_32_is_full(s));
					break;
				case 48:
					MALLOC_ASSERT(!packed_slab_is_empty(s));
					MALLOC_ASSERT(!packed_slab_48_is_full(s));
					break;
				default:
					MALLOC_ASSERT(!packed_slab_is_empty(s));
					MALLOC_ASSERT(!packed_slab_is_full(s));
					break;
			}

			// don't expect more than 1000000 slabs in any bin, likely a loop
			MALLOC_ASSERT(cnt < 1000000);
			prev_slab = s;
		}

		MALLOC_ASSERT(smallbin_cnts[bin_idx] == cnt);
	}

	// iterate through the medium bins, verifying that they only contain blocks
	// of the correct size and that there are a correct number of them
	for (uint32_t bin_idx = 0; bin_idx < NUM_MEDIUMBINS; bin_idx++) {
		uint32_t cnt = 0;

		block_t ** bin = &h->mediumbins[bin_idx];
		block_t * prev_b = medium_bin_start(bin);
		for (block_t * b = *bin; b != NULL; b = b->next) {
			cnt++;
			MALLOC_ASSERT(b->prev == prev_b);
			slab_t * s = block_get_slab(b);
			uint8_t block_idx = medium_bin_find_block_pos(s, b);
			MALLOC_ASSERT(medium_bin_idx(medium_bin_block_size(s, block_idx)) == bin_idx);

			// no empty slabs allowed
			MALLOC_ASSERT(s->block_alloc != 0);

			// don't expect more than 1000000 blocks in any bin, likely a loop
			MALLOC_ASSERT(cnt < 1000000);
			prev_b = b;
		}

		MALLOC_ASSERT(mediumbin_cnts[bin_idx] == cnt);

		uint8_t s_idx = medium_bin_skiplist_idx(bin_idx);
		uint8_t s_bit = medium_bin_skiplist_bit(bin_idx);
		MALLOC_ASSERT(cnt == 0 || ((h->med_skiplist[s_idx] >> s_bit) & 1));
	}

	return true;
}


static const char * bool_str(int b) {
	static const char * strs[] = {
		"false", "true"
	};
	return strs[b];
}

static const char * bitv_str(uint64_t bitv, int len) {
	static char buf[67];

	buf[0] = '[';
	for (int i = 0; i < len; i++) {
		buf[i + 1] = '0' + ((bitv >> i) & 1);
	}
	buf[len + 1] = ']';
	buf[len + 2] = '\0';
	return buf;
}

static uint64_t calc_slab_used_mem(slab_t * s) {
	if (slab_is_free(s)) {
		return 0;
	}
	else if (slab_is_packed(s)) {
		uint32_t size = packed_slab_block_size(s);
		uint32_t cnt = 0;

		uint32_t block_sz = packed_slab_block_size(s);
		switch (block_sz) {
			case 16:
				for (int i = 0; i < CEIL_DIV(PACKED_SLAB_16_BITV_LEN, 8); i++) {
					uint64_t bitv = ((uint64_t *) s->ps_f2)[i];
					cnt += __builtin_popcountl(bitv);
				}
				cnt = PACKED_SLAB_16_BITV_BITS - cnt;
				break;
			case 32:
				for (int i = 0; i < CEIL_DIV(PACKED_SLAB_32_BITV_LEN, 8); i++) {
					uint64_t bitv = ((uint64_t *) s->ps_f2)[i];
					cnt += __builtin_popcountl(bitv);
				}
				cnt = PACKED_SLAB_32_BITV_BITS - cnt;
				break;
			case 48:
				for (int i = 0; i < CEIL_DIV(PACKED_SLAB_48_BITV_LEN, 8); i++) {
					uint64_t bitv = ((uint64_t *) s->ps_f2)[i];
					cnt += __builtin_popcountl(bitv);
				}
				cnt = PACKED_SLAB_48_BITV_BITS - cnt;
				break;
			default:
				// larger chunk sizes (64+ bytes)
				cnt += __builtin_popcountl(s->pl_f);
				cnt = packed_slab_n_blocks(size) - cnt;
				break;
		}
		return cnt * size;
	}
	else {
		// large slab
		uint64_t cnt = 0;
		uint64_t size = large_slab_get_size(s);
		uint64_t block_sz;

		uint32_t offset = LARGE_SLAB_HEADER_SIZE;
		uint32_t next_off;
		int i = 0;
		// iterate through smaller blocks
		do {
			next_off = s->block_offs[i] * MALLOC_ALIGN;
			int alloc = ((s->block_alloc >> i) & 1);

			if (i == 7 || next_off == 0) {
				if (i < 7 && (s->block_alloc & (2 << i))) {
					block_sz = SLAB_SIZE - offset;
				}
				else {
					// continues to the end
					block_sz = size * SLAB_SIZE - offset;
				}
			}
			else {
				block_sz = next_off - offset;
			}

			cnt += alloc ? block_sz : 0;

			offset = next_off;
			i++;
		} while (i < 8 && offset != 0);

		if (size > 1 && i < 7 && offset == 0 &&
				(s->block_alloc & (1 << i))) {
			// there is a block starting at the very end of the first slab
			cnt += SLAB_SIZE * (size - 1);
		}
		return cnt;
	}
}

static void print_slab(slab_t * s) {
	printf( "slab at " BOLD "0x%p:" P_RESET "\n"
			"\tfree: %s\n",
			s,
			bool_str(slab_is_free(s)));

	if (slab_is_free(s)) {
		printf( "\tsize: %ld\n",
				free_slab_size(s));
	}
	else if (slab_is_packed(s)) {
		printf( "\tis packed: true\n"
				"\tblock size: %d\n",
				packed_slab_block_size(s));

		uint32_t block_sz = packed_slab_block_size(s);
		printf("\tbitv:");
		switch (block_sz) {
			case 16:
				for (int i = 0; i < CEIL_DIV(PACKED_SLAB_16_BITV_LEN, 8); i++) {
					printf( "\n"
							"\t\t%s",
							bitv_str(((uint64_t *) s->ps_f2)[i], 64));
				}
				break;
			case 32:
				for (int i = 0; i < CEIL_DIV(PACKED_SLAB_32_BITV_LEN, 8); i++) {
					printf( "\n"
							"\t\t%s",
							bitv_str(((uint64_t *) s->ps_f2)[i], 64));
				}
				break;
			case 48:
				for (int i = 0; i < CEIL_DIV(PACKED_SLAB_48_BITV_LEN, 8); i++) {
					printf( "\n"
							"\t\t%s",
							bitv_str(((uint64_t *) s->ps_f2)[i], 64));
				}
				break;
			default:
				// larger chunk sizes (64+ bytes)
				printf(" %s", bitv_str(s->pl_f, packed_slab_n_blocks(block_sz)));
				break;
		}
		printf("\n");
	}
	else {
		// large slab
		uint64_t size = large_slab_get_size(s);
		uint64_t block_sz;
		printf( "\toffset (size): %lu\n"
				"\tblock_alloc: %02x\n"
				"\tblock_offs: %016lx\n",
				size,
				s->block_alloc,
				__medium_bin_offsets(s));

		uint32_t offset = LARGE_SLAB_HEADER_SIZE;
		uint32_t next_off;
		int i = 0;
		// iterate through smaller blocks
		do {
			next_off = s->block_offs[i] * MALLOC_ALIGN;
			int alloc = ((s->block_alloc >> i) & 1);

			if (i == 7 || next_off == 0) {
				if (i < 7 && (s->block_alloc & (2 << i))) {
					block_sz = SLAB_SIZE - offset;
				}
				else {
					// continues to the end
					block_sz = size * SLAB_SIZE - offset;
				}
			}
			else {
				block_sz = next_off - offset;
			}

			printf("\t\tblock: alloc: %s\tsize: %lu,\toffset: %u\n",
					bool_str(alloc), block_sz, offset);

			offset = next_off;

			i++;
		} while (i < 8 && offset != 0);

		if (size > 1 && i < 7 && offset == 0 &&
				(s->block_alloc & (1 << i))) {
			// there is a block starting at the very end of the first slab
			printf("\t\tblock: alloc: %s\tsize: %lu,\toffset: %u\n",
					bool_str(1), SLAB_SIZE * (size - 1), SLAB_SIZE);
		}
	}
}

static void print_slab_cond(slab_t * s) {
	uint64_t used_mem = calc_slab_used_mem(s);


	printf( "slab at 0x%p: f: %s, ",
			s, bool_str(slab_is_free(s)));

	if (slab_is_free(s)) {
		printf( "size: %ld\n",
				free_slab_size(s));
	}
	else if (slab_is_packed(s)) {
		double util = ((double) used_mem) / SLAB_SIZE;
		printf( "bsize: %d, util: %f\n",
				packed_slab_block_size(s),
				util);

		if (util == 1) {
			print_slab(s);
		}
	}
	else {
		uint64_t size = large_slab_get_size(s);
		double util = ((double) used_mem) / (size * SLAB_SIZE);
		printf( "offset: %lu, util: %f\n",
				size, util);

		if (util < .25) {
			print_slab(s);
		}
	}
}

static void _print_heap() {
	slab_t * s = (slab_t *) 0x80000a000;
	if (s < heap->heap_end) {
		print_slab(s);
	}
}

static void print_heap() {
	heap_t * h = heap;

	printf("flat memory layout:\nheap_end: %p\n",
			heap_end(h));
	for (slab_t * s = heap_start(h); s != heap_end(h); s = next_adj_slab(s)) {
		print_slab_cond(s);
		if (next_adj_slab(s) == s ||
				((ptr_int_t) next_adj_slab(s) > ((ptr_int_t) h->heap_end))) {
			// prevent infinite loop
			break;
		}
	}

	uint64_t mem = 0;
	// calculate fragmentation
	for (slab_t * s = heap_start(h); s != heap_end(h); s = next_adj_slab(s)) {
		mem += calc_slab_used_mem(s);
		if (next_adj_slab(s) == s ||
				((ptr_int_t) next_adj_slab(s) > ((ptr_int_t) h->heap_end))) {
			// prevent infinite loop
			break;
		}
	}

	printf( "total used mem: %lu\n"
			"heap size: %lu\n"
			"utilization: %f\n",
			mem,
			(((ptr_int_t) heap_end(h)) - ((ptr_int_t) h)),
			((double) mem) / ((double) (((ptr_int_t) heap_end(h)) - ((ptr_int_t) h))));

	/*
	// print free slabs
	printf("slab bins:\n");
	for (size_t idx = 0; idx < NUM_SLAB_BINS; idx++) {
		slab_t * s = h->slab_bins[idx];
		size_t size = (1 << idx) * SLAB_SIZE;
		if (s != NULL) {
			printf("\t[%zu]:\n", size);
		}
		for (; s != NULL; s = s->next) {
			printf("\t\tslab " BOLD "0x%p" P_RESET "\n", s);
		}
	}

	// print smallbins
	printf("smallbins:\n");
	for (size_t idx = 0; idx < NUM_SMALLBINS; idx++) {
		slab_t * s = h->smallbins[idx];
		if (s != NULL) {
			printf("\t[%3zu]:\n", (idx * MALLOC_ALIGN) + MALLOC_ALIGN);
		}
		for (; s != NULL; s = s->next_p) {
			printf("\t\tslab " BOLD "0x%p" P_RESET "\n", s);
		}
	}

	// print mediumbins
	printf("mediumbins:\n");
	for (size_t idx = 0; idx < NUM_MEDIUMBINS; idx++) {
		block_t * b = h->mediumbins[idx].head;
		if (b != &h->mediumbins[idx]) {
			printf("\t[%3zu]:\n", (idx * MALLOC_ALIGN) + MIN_MEDIUM_BLOCK_SZ);
		}
		for (; b != &h->mediumbins[idx]; b = b->next) {
			printf("\t\tblock " BOLD "0x%p" P_RESET "\n", b);
		}
	}

	printf("medium skiplist:\n");
	for (size_t idx = 0; idx < MEDIUMBINS_SKIPLIST_SZ; idx++) {
		printf("\t\t%s\n", bitv_str(h->med_skiplist[idx], 32));
	}*/
}

#endif /* DEFINE_CHECKS */

