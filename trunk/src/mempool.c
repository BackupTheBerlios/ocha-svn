#include "mempool.h"
#include <glib.h>

struct mempool
{
   size_t size;
   GSList *chunks;
};

struct mempool *mempool_new()
{
   struct mempool* retval = g_malloc(sizeof(struct mempool));
   retval->size=0;
   retval->chunks=NULL;
   return retval;
}

gpointer mempool_alloc(struct mempool *mempool, size_t size)
{
   g_return_val_if_fail(mempool!=NULL, NULL);

   gpointer chunk = g_malloc0(size);
   
   mempool->chunks=g_slist_append(mempool->chunks, chunk);
   mempool->size+=size;
   return chunk;
}

void mempool_delete(struct mempool *mempool)
{
   g_return_if_fail(mempool!=NULL);

   if(mempool->chunks) {
	  for(GSList *current=mempool->chunks;
		  current!=NULL;
		  current=g_slist_next(current)) {
		 g_free(current->data);
	  }
	  g_slist_free(mempool->chunks);
   }
   g_free(mempool);
}

size_t mempool_size(struct mempool *mempool)
{
   g_return_val_if_fail(mempool!=NULL, 0);
   return mempool->size;
}
