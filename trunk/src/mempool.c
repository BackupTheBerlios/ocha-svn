#include "mempool.h"
#include <glib.h>

struct mempool
{
   size_t size;
   GSList *chunks;
};
#define MAGIC_NUMBER 0xfeeddead

struct mempool_header
{	
   /** function to free the resource, never null. */
   mempool_freer_f freer;
   /** the resource to free */
   gpointer resource;
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
   if(size==0)
	  return NULL;
   size_t real_size = size + sizeof(struct mempool_header);
   struct mempool_header* header = g_malloc0(real_size);
   header->freer=g_free;
   header->resource=header;
   mempool->chunks=g_slist_prepend(mempool->chunks, header);
   mempool->size+=real_size;
   gpointer chunk = ((gint8*)header)+sizeof(struct mempool_header);
   return chunk;
}

void mempool_delete(struct mempool *mempool)
{
   g_return_if_fail(mempool!=NULL);

   if(mempool->chunks) {
	  for(GSList *current=mempool->chunks;
		  current!=NULL;
		  current=g_slist_next(current)) {
		 struct mempool_header *header = (struct mempool_header *)current->data;
		 header->freer(header->resource);
	  }
	  g_slist_free(mempool->chunks);
   }
   g_free(mempool);
}

void mempool_enlist(struct mempool *mempool, void *resource, mempool_freer_f freer)
{
   g_return_if_fail(mempool!=NULL);
   g_return_if_fail(freer!=NULL);

   struct mempool_header *header = (struct mempool_header*)mempool_alloc(mempool, 
																		 sizeof(struct mempool_header));
   header->resource=resource;
   header->freer=freer;
   mempool->chunks=g_slist_prepend(mempool->chunks, header);
}

size_t mempool_size(struct mempool *mempool)
{
   g_return_val_if_fail(mempool!=NULL, 0);
   return mempool->size;
}
