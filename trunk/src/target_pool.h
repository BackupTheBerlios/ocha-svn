#ifndef TARGET_POOL_H
#define TARGET_POOL_H

#include "target.h"

/** \file
 * A target pool keeps track of all targets currently
 * available in the application. It avoids re-creating
 * the same target over and over again. It can also be
 * used to check whether a target is still in use in
 * any other part of the application.
 *
 * A target pool usualy does not keep a reference on
 * the target so as to make it possible for unreferenced
 * target to be freed. However, a pool may choose to
 * keep a reference on some target for some time to
 * avoid re-creating them.
 *
 * target pools are protected by a mutex. Only one
 * thread can modify or access a target pool at a 
 * time.
 */

/**
 * Create a new target pool.
 *
 * @return a new target pool (never null, this function fails if 
 * there's not enough memory)
 */
struct target_pool *target_pool_new(void);

/**
 * Delete a target pool.
 *
 * This frees all resources used by the target pool and
 * remove all references still held on some targets
 */
void target_pool_delete(struct target_pool *);

/**
 * Lookup a target and CREATE A NEW REFERENCE TO IT.
 *
 * This method lookup a target and if the lookup is successful,
 * it will return a new reference to the target. The caller
 * is responsible for calling target_unref() on the target
 * returned by this function. Not doing so may result in
 * targets never being freed.
 *
 * @param target_pool
 * @param id identifier for the target
 * @return target a new reference to an existing target or NULL
 */
struct target *target_pool_get(struct target_pool *, const char *id);

/**
 * Add a target into the pool.
 *
 * This function adds a new target into the pool. No new reference
 * is usually made to the target and it can be safely freed
 * after this method has been returned. The caller must hold a
 * reference for the duration of this call, however. 
 *
 * If a target with the same id already exists on the pool, it
 * is overwritten.
 *
 * @param target_pool
 * @param target the target to add
 */
void target_pool_register(struct target_pool *, struct target *);

#endif
