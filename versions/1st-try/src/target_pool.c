#include <stdlib.h>
#include <glib.h>
#include "target_pool.h"

/** \file 
 * Implementation of API defined in target_pool.h
 */

struct target_pool
{
   /** string x target */
   GHashTable* table;

   /** Mutex for the whole pool.
	* The mutex should be locked before accessing anything
	* else in this structure
	*/
   GStaticMutex mutex;

   /** Number of target_freer still keeping a reference to the pool */
   int freer_data_num;

   /** The target pool should be deleted once table_entries reaches 0. 
	* When the target pool is deleted while it still contains targets
	* with references on it (through mempool_enlist), then
	* deletion of the target pool is delayed until the last target
	* on the pool has been destroyed. This flag marks this state.
	*/
   bool deleted;
};

struct target_pool *target_pool_new()
{
   struct target_pool *retval = g_malloc0(sizeof(struct target_pool));
   retval->table = g_hash_table_new(g_str_hash, g_str_equal);
   g_static_mutex_init(&retval->mutex);
   retval->deleted=false;
   return retval;
}

/**
 * Really delete the pool and its content.
 * 
 * The mutex should be locked before calling this function.
 */
static void target_pool_really_delete(struct target_pool *pool) 
{
   g_return_if_fail(pool->freer_data_num==0);
   g_hash_table_destroy(pool->table);
   g_static_mutex_free(&pool->mutex);
   g_free(pool);
}

void target_pool_delete(struct target_pool *pool)
{
   g_return_if_fail(pool!=NULL);
   g_static_mutex_lock(&pool->mutex);

   if(pool->freer_data_num==0) {
	  target_pool_really_delete(pool);
	  return; /* don't unlock the mutex */
   } else {
	  pool->deleted=true;
   }
   g_static_mutex_unlock(&pool->mutex);
}

struct target *target_pool_get(struct target_pool *pool, const char *id)
{
   g_return_val_if_fail(pool!=NULL, NULL);
   g_static_mutex_lock(&pool->mutex);
   struct target *target = g_hash_table_lookup(pool->table, id);
   if(target!=NULL) 
	  target_ref(target);
   g_static_mutex_unlock(&pool->mutex);
   return target;
}

static void target_pool_freer(struct target *target, gpointer data)
{
   g_return_if_fail(target!=NULL);
   g_return_if_fail(data!=NULL);
   struct target_pool *pool=(struct target_pool *)data;

   g_static_mutex_lock(&pool->mutex);
   
   const char *id=target->id;
   if(g_hash_table_lookup(pool->table, id)==target) {
	  g_hash_table_remove(pool->table, id);
   }

   pool->freer_data_num--;

   if(pool->deleted && pool->freer_data_num==0) {
	  target_pool_really_delete(pool);
	  return; /* don't unlock the mutex */
   }
   g_static_mutex_unlock(&pool->mutex);
}

void target_pool_register(struct target_pool *pool, struct target *target)
{
   g_return_if_fail(pool!=NULL);
   g_return_if_fail(target!=NULL);   
   g_static_mutex_lock(&pool->mutex);
   const char *id = target->id;
   if(g_hash_table_lookup(pool->table, id)!=target) {
	  g_hash_table_replace(pool->table, (gpointer)id, target);
	  pool->freer_data_num++;
	  target_enlist(target, 
							pool,
							target_pool_freer);
   }
   g_static_mutex_unlock(&pool->mutex);
}

