#ifndef TARGET_H
#define TARGET_H

#include <glib.h>
#include "mempool.h"

/**
 * An object on which actions can be performed.
 * 
 * A target is the basic object type
 * on which ocha works. It represents some typed
 * data on which some actions can be executed. What
 * action can be executed depend on the type of
 * the target or even the catalog the target comes
 * from. 
 * 
 * Targets are reference-counted.
 * 
 * A target can be marked as invalid, in which case
 * it should not be used because it looks like it's
 * actually not what the user wanted. Invalid target
 * still work normally, though; it's just a flag. 
 * Invalid targets should be discarded ASAP. 
 *
 * All string pointers should be allocated from
 * the target's memory pool so that they'll be
 * discarded at the same time as the target.
 *
 * This structure only lists the public fields
 * of a target. 
 */
struct target 
{
   /** A user-friendly name for this target */
   const gchar* name;

   /** Mimetype of the target, NULL if not known. */
   const gchar *mimetype;

   /** Absolute path to the file if relevant, NULL otherwise. */
   const gchar *filepath;

   /** URL to the file if relevant, NULL otherwise. */
   const gchar *url;
};

/**
 * Action types.
 * 
 * Action type will be used to display the action
 * properly or to choose (prioritize) actions. It 
 * has no other meaning.
 */
enum target_action_type 
{
   ACTION_VIEW,
   ACTION_EDIT
};

/**
 * An action that can be executed on a target.
 * 
 * An action is identified by a name, a type
 * and a callback method. 
 * 
 * Actions should be allocated from their target
 * memory pool if they're linked to the target. 
 */
struct target_action
{												
   const gchar *name;
   enum target_action_type type;
   target_action_f call;
};

/**
 * Action callback.
 * 
 * This function will be called when an action needs to
 * be executed. It is passed the action structure which
 * can contain any data following the target_action 
 * structure.
 *
 * @param target_action the structure where this callback was found
 * @param target target on which the action should be executed
 */
typedef void (*target_action_f)(struct target_action*, struct target*);

/**
 * Create a new target.
 *
 * If not enough memory could be allocated, the
 * program terminates, glib-style.
 *
 * @param name name for this target (required). The content of this
 * string will be copied.
 * @return a newly-allocated target structure, with its reference
 * count set to 1. never null
 */
struct target *target_new(const gchar* name);

/**
 * Create a new reference to a target.
 *
 * @param target 
 * @return a new reference to the same target
 */
struct target *target_ref(struct target* target);

/**
 * Remove one reference to the target.
 *
 * When the reference count reaches 0, the target is deleted. The
 * pointer to the target should be considered invalid after this
 * call. The pointer should be discarded, ideally reset to NULL.
 *
 * @param target 
 */
void targed_unref(struct target* target);

/**
 * Get an array of actions defined for the target.
 *
 * This function fills an array of fixed size with the actions defined for the target.
 * If the array is too small, only as many actions as fits into the array will be
 * copied. If the array is too large, only as many actions as necessary will be
 * copied into the array and the rest will be filled with 0. 
 * 
 * @param target 
 * @param target_actions an array of target_actions to fill, may be null if action_dest_size==0
 * @param action_dest_size size of the array target_actions
 * @return total number of actions, which might be >action_dest_size
 */
unsigned int target_get_actions(struct target* target, struct target_action *target_actions, unsigned int action_dest_size);

/**
x * Add an action into a target.
 *
 * The target_action will be added into the target action array as-is. It will
 * not be copied. If you need it to be freed at the same time as the target,
 * then allocate the memory for the target action from the target's memory
 * pool.
 *
 * @param target
 * @param action action to add into the target
 */
void target_add_action(struct target* target, struct target_action* target_action);

/**
 * Remove an action from a target.
 *
 * If the action is found in the target, it is removed and the function returns
 * true. Otherwise, the function returns false. 
 *
 * The memory of the target action is not freed by this call.
 *
 * @param target
 * @param target_action 
 * @return true if an action was removed
 */
bool target_remove_action(struct target* target, struct target_action* target_action);
#endif
