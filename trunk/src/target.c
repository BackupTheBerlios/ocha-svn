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
   const gchar* mimetype;
   int refs;
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

static gchar *target_strdup(struct target* target, const char *str)
{
   gchar *dup = (gchar *)mempool_alloc(target->mempool,
									   strlen(str)+1);
   strcpy(dup, str);
   return dup;
}
static struct full_target *target_new()
{
   struct mempool *pool = mempool_new();
   struct full_target *target = mempool_alloc_type(pool, struct full_target);
   target->target.mempool = pool;
   target->refs=1;
   return target;
}
static void target_delete(struct full_target *target)
{
   mempool_delete(target->target.mempool);
}

struct target *target_new_file_target(const gchar* filepath)
{
   g_return_val_if_fail(filepath!=NULL, NULL);
   const gchar *basename = strrchr(filepath, '/');
   if(!basename)
	  basename=filepath;
   struct target *target = to_target(target_new());
   gchar *url = (gchar*)mempool_alloc(target->mempool,
									  strlen(FILE_URL_HEADER)
									  +strlen(filepath)
									  +1);
   strcpy(url, FILE_URL_HEADER);
   strcat(url, filepath);
   
   target->id=url;
   target->url=url;
   target->name=target_strdup(target, basename);
   target->filepath=&url[strlen(FILE_URL_HEADER)];
   return target;
}

struct target *target_new_url_target(const gchar* name, const gchar* url)
{
   g_return_val_if_fail(name!=NULL, NULL);
   g_return_val_if_fail(url!=NULL, NULL);

   struct target *target = to_target(target_new());
   gchar *c_url = target_strdup(target, url);

   target->id=c_url;
   target->name=target_strdup(target, name);
   target->url=c_url;
   if(strncmp(FILE_URL_HEADER, c_url, strlen(FILE_URL_HEADER))==0)
	  target->filepath=&c_url[strlen(FILE_URL_HEADER)];
   return target;
}

struct target *target_ref(struct target* target)
{
   g_return_val_if_fail(target, NULL);
   struct full_target *full = to_full_target(target);
   full->refs++;
   return target;
}

void target_unref(struct target* target)
{
   g_return_if_fail(target);
   struct full_target *full = to_full_target(target);
   full->refs--;
   int refs = full->refs;
   if(refs<0) 
	  g_warning("reference below 0 (%d) in target 0x%lx\n", refs, target);
   else if(refs==0)
	  target_delete(full);
}

const gchar *target_get_mimetype(struct target* target)
{
   g_return_val_if_fail(target, NULL);
   return to_full_target(target)->mimetype;
}

void target_set_mimetype(struct target* target, const gchar *mimetype)
{
   struct full_target *full = to_full_target(target);
   if(mimetype)
	  full->mimetype=target_strdup(target, mimetype);
   else
	  full->mimetype=NULL;
}

unsigned int target_get_actions(struct target* target, struct target_action *target_actions, unsigned int action_dest_size)
{
   return 0;
}

void target_add_action(struct target* target, struct target_action* target_action, target_action_f action_callback)
{
   return;
}

void target_execute_action(struct target* target, struct target_action* target_action)
{
   return;
}
 
bool target_remove_action(struct target* target, struct target_action* target_action)
{
   return false;
}
