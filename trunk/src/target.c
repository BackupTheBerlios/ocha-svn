#include <stdlib.h>
#include <string.h>
#include "target.h"

#define FILE_URL_HEADER "file://"

/**
 * The complete target structure.
 *
 * struct full_target starts with a struct target, so
 * it's possible to cast from one to the other in this
 * module.
 */
struct full_target 
{
   struct target target;
   struct mempool *mempool;
   const gchar* mimetype;
   GArray *actions;
   int refs;
   GStaticMutex mutex;
};
#define LOCK(target) g_static_mutex_lock(&((struct full_target *)target)->mutex)
#define UNLOCK(target) g_static_mutex_unlock(&((struct full_target *)target)->mutex)

struct action_ref
{
   struct target_action *ref;
   target_action_f call;
};

static inline struct target* to_target(struct full_target *full)
{
   g_return_val_if_fail(full, NULL);
   return (struct target*)full;
}
static inline struct full_target* to_full_target(struct target *target)
{
   g_return_val_if_fail(target, NULL);
   return (struct full_target*)target;
}

static gchar *target_strdup(struct full_target* target, const char *str)
{
   gchar *dup = (gchar *)mempool_alloc(target->mempool,
									   strlen(str)+1);
   strcpy(dup, str);
   return dup;
}
static void target_free_array(GArray* array)
{
   g_array_free(array, true/*free segment*/);
}
static struct full_target *target_new()
{
   struct mempool *pool = mempool_new();
   struct full_target *target = mempool_alloc_type(pool, struct full_target);
   target->mempool = pool;
   target->refs=1;
   GArray* actions = g_array_new(false/*zero_terminated*/, 
									false/*clear*/,
									sizeof(struct action_ref)/*element_size*/);
   mempool_enlist(target->mempool, 
				  actions,
				  (mempool_freer_f)target_free_array);
   target->actions=actions;

   g_static_mutex_init(&target->mutex);
   return target;
}
static void target_delete(struct full_target *target)
{
   mempool_delete(target->mempool);
}

struct target *target_new_file_target(const gchar* filepath)
{
   g_return_val_if_fail(filepath!=NULL, NULL);
   const gchar *basename = strrchr(filepath, '/');
   if(!basename)
	  basename=filepath;
   struct full_target *full_target = target_new();
   struct target *target = to_target(full_target);
   gchar *url = (gchar*)mempool_alloc(full_target->mempool,
									  strlen(FILE_URL_HEADER)
									  +strlen(filepath)
									  +1);
   strcpy(url, FILE_URL_HEADER);
   strcat(url, filepath);
   
   target->id=url;
   target->url=url;
   target->name=target_strdup(full_target, basename);
   target->filepath=&url[strlen(FILE_URL_HEADER)];
   return target;
}

struct target *target_new_url_target(const gchar* name, const gchar* url)
{
   g_return_val_if_fail(name!=NULL, NULL);
   g_return_val_if_fail(url!=NULL, NULL);

   struct full_target *full_target = target_new();
   struct target *target = to_target(full_target);
   gchar *c_url = target_strdup(full_target, url);

   target->id=c_url;
   target->name=target_strdup(full_target, name);
   target->url=c_url;
   if(strncmp(FILE_URL_HEADER, c_url, strlen(FILE_URL_HEADER))==0)
	  target->filepath=&c_url[strlen(FILE_URL_HEADER)];
   return target;
}

struct target *target_ref(struct target* target)
{
   g_return_val_if_fail(target, NULL);
   struct full_target *full = to_full_target(target);

   LOCK(target);
   full->refs++;
   UNLOCK(target);

   return target;
}

void target_unref(struct target* target)
{
   g_return_if_fail(target);
   struct full_target *full = to_full_target(target);
   
   LOCK(target);
   full->refs--;
   int refs = full->refs;
   if(refs<0) 
	  g_warning("reference below 0 (%d) in target 0x%lx\n", refs, target);
   else if(refs==0) {
	  target_delete(full);
	  g_static_mutex_free(&full->mutex);
	  return;
   } 
   UNLOCK(target);
}

const gchar *target_get_mimetype(struct target* target)
{
   g_return_val_if_fail(target, NULL);					

   LOCK(target);
   const gchar *retval = to_full_target(target)->mimetype;
   UNLOCK(target);
   return retval;
}

void target_set_mimetype(struct target* target, const gchar *mimetype)
{
   struct full_target *full = to_full_target(target);

   LOCK(target);
   if(mimetype)
	  full->mimetype=target_strdup(full, mimetype);
   else
	  full->mimetype=NULL;
   UNLOCK(target);
}

unsigned int target_get_actions(struct target* target, struct target_action **target_actions, unsigned int action_dest_size)
{
   g_return_val_if_fail(target!=NULL, 0);

   LOCK(target);
   GArray* actions = to_full_target(target)->actions;
   int len = actions->len;
   if(target_actions) {

	  guint tocopy = action_dest_size;
	  if(len<tocopy)
		 tocopy=len;

	  for(int i=0; i<tocopy; i++) {
		 struct action_ref *ref = &g_array_index(actions, 
												 struct action_ref, 
												 i);
		 target_actions[i]=ref->ref;
	  }
	  for(int i=tocopy; i<action_dest_size; i++)
		 target_actions[i]=NULL;

   }
   UNLOCK(target);

   return len;
}

void target_add_action(struct target* target, struct target_action* target_action, target_action_f action_callback)
{
   g_return_if_fail(target!=NULL);
   g_return_if_fail(target_action!=NULL);
   g_return_if_fail(action_callback!=NULL);


   struct full_target *full_target = to_full_target(target);

   LOCK(target);
   GArray* actions = full_target->actions;
   g_array_set_size(actions, actions->len+1);
   struct action_ref *ref= &((struct action_ref*)actions->data)[actions->len-1];
   ref->ref = target_action;
   ref->call = action_callback;
   UNLOCK(target);
}

static int target_find_action_ref_index(struct full_target *target, struct target_action *action)
{
   struct action_ref *refs = (struct action_ref *)target->actions->data;
   int len = target->actions->len;
   for(int i=0; i<len; i++) {
	  if(refs[i].ref==action)
		 return i;
   }
   return -1;
}


bool target_execute_action(struct target* target, struct target_action* target_action)
{
   g_return_val_if_fail(target!=NULL, false);
   g_return_val_if_fail(target_action!=NULL, false);

   struct full_target *full_target = to_full_target(target);

   LOCK(target);
   int index = target_find_action_ref_index(full_target, target_action);
   if(index>=0) {
	  struct action_ref *ref = &g_array_index(full_target->actions, 
											  struct action_ref, 
											  index);
	  target_action_f call = ref->call;
	  UNLOCK(target);

	  call(target, target_action);
	  return true;
   } else {
	  UNLOCK(target);
	  return false;
   }
}
 
bool target_remove_action(struct target* target, struct target_action* target_action)
{
   g_return_val_if_fail(target!=NULL, false);
   g_return_val_if_fail(target_action!=NULL, false);

   struct full_target *full_target = to_full_target(target);

   LOCK(target);
   int index = target_find_action_ref_index(full_target, target_action);
   bool retval;
   if(index>=0) {
	  g_array_remove_index(full_target->actions, index);
	  retval=true; 
   } else {
	  retval=false;
   }
   UNLOCK(target);
   return retval;
}
  
gpointer target_mempool_alloc(struct target* target, size_t size)
{
   g_return_val_if_fail(target!=NULL, NULL);

   struct full_target *full_target = to_full_target(target);
   gpointer retval;

   LOCK(target);
   retval = mempool_alloc(full_target->mempool, size);
   UNLOCK(target);

   return retval;
}

void target_mempool_enlist(struct target *target, gpointer resource, mempool_freer_f freer)
{
   g_return_if_fail(target!=NULL);
   struct full_target *full_target = to_full_target(target);

   LOCK(target);
   mempool_enlist(full_target->mempool, resource, freer);
   UNLOCK(target);
}
