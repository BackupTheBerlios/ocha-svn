#ifndef MEMPOOL_H
#define MEMPOOL_H
#include <stdlib.h>

/** \file
 * Memory pool make it possible to free memory allocated
 * all over the place in one bunch.
 * 
 * It is not possible to free memory separately, so
 * there's no function for that.
 */

/**
 * Create a new memory pool
 * 
 * If not enough memory could be allocated, the
 * program terminates, glib-style.
 * @return a new memory pool 
 */
struct mempool *mempool_new();

/**
 * Get the current size of the pool.
 * 
 * The size, at any time, is equal to
 * or larger than the total size of
 * the allocated chunks.
 */
size_t mempool_size();

/**
 * Allocate some memory from the memory pool.
 * 
 * If allocation fails, the program terminates (glib-style)
 * @return a chunck of at least size bits, initialized to 0 
 */
void *mempool_alloc(struct mempool *, size_t size);

/**
 * Method used to free a resource.
 *
 */
typedef void (*mempool_freer_f)(void *);

/**
 * Add some resource to the pool so that it'll be freed
 * when the pool is destroyed.
 *
 * @param mempool
 * @param pointer pointer to the resource 
 * @param freer function that will free the resource, usually
 * some flavour of free()
 */
void mempool_enlist(struct mempool *, void *resource, mempool_freer_f freer);

/**
 * Discard a memory pool and all the memory allocated
 * for it.
 *
 * @param mempool momory pool
 */
void mempool_delete(struct mempool *);

#endif
