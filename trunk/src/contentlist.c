#include "contentlist.h"
#include <string.h>
/** \file Custom implementation of a GtkTreeModel that stores entries
 *
 */

typedef struct _ContentListEntry ContentListEntry;
/** An entry, as seen by the list */
struct _ContentListEntry
{
        /**
         * Whether this entry has been set.
         * as long as this is FALSE, everything else is
         * invalid.
         */
        gboolean defined;

        /** Short entry name */
        char *name;
        /** Long entry name / description */
        char *long_name;
        /** Entry ID */
        int id;
};


/**
 * The list itself (private structure)
 */
struct _ContentList
{
        /** this MUST be the first member */
        GObject         parent;

        /** Number of entries */
        guint           size;

        /** A fixed array of 'size' entries */
        ContentListEntry   *rows;

        gint            stamp;       /* Random integer to check whether an iter belongs to our model */
};



/** The GObject class */
struct _ContentListClass
{
  GObjectClass parent_class;
};

/** The string that's returned when an entry is undefined */
#define UNDEFINED_STRING "..."

/* ------------------------- prototypes: class functions(customlist_class) */
static void contentlist_class_init(ContentListClass *klass);
static void contentlist_class_implements(GtkTreeModelIface *iface);
/* ------------------------- prototypes: member functions(customlist) */
static void contentlist_init(ContentList *pkg_tree);
static void contentlist_finalize(GObject *object);
static GtkTreeModelFlags contentlist_get_flags(GtkTreeModel *tree_model);
static gint contentlist_get_n_columns(GtkTreeModel *tree_model);
static GType contentlist_get_column_type(GtkTreeModel *tree_model, gint index);
static gboolean contentlist_get_iter(GtkTreeModel *tree_model, GtkTreeIter *iter, GtkTreePath *path);
static GtkTreePath *contentlist_get_path(GtkTreeModel *tree_model, GtkTreeIter *iter);
static void contentlist_get_value(GtkTreeModel *tree_model, GtkTreeIter *iter, gint column, GValue *value);
static gboolean contentlist_iter_next(GtkTreeModel *tree_model, GtkTreeIter *iter);
static gboolean contentlist_iter_children(GtkTreeModel *tree_model, GtkTreeIter *iter, GtkTreeIter *parent);
static gboolean contentlist_iter_has_child(GtkTreeModel *tree_model, GtkTreeIter *iter);
static gint contentlist_iter_n_children(GtkTreeModel *tree_model, GtkTreeIter *iter);
static gboolean contentlist_iter_nth_child(GtkTreeModel *tree_model, GtkTreeIter *iter, GtkTreeIter *parent, gint n);
static gboolean contentlist_iter_parent(GtkTreeModel *tree_model, GtkTreeIter *iter, GtkTreeIter *child);

/* ------------------------- prototypes: static functions */
static guint iter_get_index(ContentList *list, GtkTreeIter *iter);
static void iter_set_index(ContentList *list, GtkTreeIter *iter, guint index);
static gboolean iter_is_valid(ContentList *list, GtkTreeIter *iter);
static void send_row_changed(ContentList *list, guint index);

/* ------------------------- definitions */
static GObjectClass *parent_class = NULL;
static GType column_type[CONTENTLIST_N_COLUMNS] = {
        G_TYPE_BOOLEAN, /* DEFINED */
        G_TYPE_STRING, /* NAME */
        G_TYPE_STRING /* LONG_NAME */
};

/* ------------------------- public functions */

GType contentlist_get_type(void)
{
  static GType contentlist_type = 0;

  if(contentlist_type) {
          return contentlist_type;
  }

  /* class ContentListClass  */
  {
          static const GTypeInfo contentlist_info = {
                  sizeof(ContentListClass),
                  NULL /* base_init */,
                  NULL /* base_finalize */,
                 (GClassInitFunc) contentlist_class_init,
                  NULL /* class finalize */,
                  NULL /* class_data */,
                  sizeof(ContentList),
                  0 /* n_preallocs */,
                 (GInstanceInitFunc) contentlist_init
          };

          contentlist_type = g_type_register_static(G_TYPE_OBJECT,
                                                     "ContentList",
                                                     &contentlist_info,
                                                    (GTypeFlags)0);
  }

  /* implements GtkTreeModel */
  {
          static const GInterfaceInfo tree_model_info = {
                 (GInterfaceInitFunc) contentlist_class_implements,
                  NULL,
                  NULL
          };

          g_type_add_interface_static(contentlist_type,
                                       GTK_TYPE_TREE_MODEL,
                                       &tree_model_info);
  }

  return contentlist_type;
}

ContentList *contentlist_new(guint size)
{
        ContentList *retval;

        retval =(ContentList *)g_object_new(CONTENTLIST_TYPE, NULL);
        retval->rows=g_new(ContentListEntry, size);
        memset(retval->rows, 0, size*sizeof(ContentListEntry));
        retval->size=size;
        return retval;
}

void contentlist_set(ContentList *contentlist, guint index, int id, const char *name, const char *long_name)
{
        ContentListEntry *entry;

        g_return_if_fail(contentlist);
        g_return_if_fail(IS_CONTENTLIST(contentlist));
        g_return_if_fail(index<contentlist->size);
        g_return_if_fail(name);
        g_return_if_fail(long_name);

        entry = &contentlist->rows[index];
        if(!entry->defined) {
                entry->name=g_strdup(name);
                entry->long_name=g_strdup(long_name);
                entry->id=id;
                entry->defined=TRUE;
                send_row_changed(contentlist, index);
        }
}

gboolean contentlist_get(ContentList *contentlist, guint index, int *id_out, char **name_out, char **long_name_out)
{
        ContentListEntry *entry;

        g_return_val_if_fail(contentlist, FALSE);
        g_return_val_if_fail(IS_CONTENTLIST(contentlist), FALSE);
        g_return_val_if_fail(index<contentlist->size, FALSE);

        entry = &contentlist->rows[index];
        if(!entry->defined) {
                return FALSE;
        } else {
                if(id_out) {
                        *id_out=entry->id;
                }
                if(name_out) {
                        *name_out=entry->name;
                }
                if(long_name_out) {
                        *long_name_out=entry->long_name;
                }
                return TRUE;
        }
}

void contentlist_set_at_iter(ContentList *contentlist, GtkTreeIter *iter, int id, const char *name, const char *long_name)
{
        g_return_if_fail(contentlist);
        g_return_if_fail(iter);
        g_return_if_fail(IS_CONTENTLIST(contentlist));
        contentlist_set(contentlist,
                        iter_get_index(contentlist, iter),
                        id,
                        name,
                        long_name);
}

gboolean contentlist_get_at_iter(ContentList *contentlist, GtkTreeIter *iter, int *id_out, char **name, char **long_name)
{
        g_return_val_if_fail(contentlist, FALSE);
        g_return_val_if_fail(iter, FALSE);
        g_return_val_if_fail(IS_CONTENTLIST(contentlist), FALSE);
        return contentlist_get(contentlist,
                               iter_get_index(contentlist, iter),
                               id_out,
                               name,
                               long_name);
}

guint contentlist_get_size(ContentList *contentlist)
{
        g_return_val_if_fail(contentlist, FALSE);
        g_return_val_if_fail(IS_CONTENTLIST(contentlist), FALSE);
        return contentlist->size;
}


/* ------------------------- class functions(customlist_class) */
/** Init callbacks for the gobject type system */
static void contentlist_class_init(ContentListClass *klass)
{
        GObjectClass *object_class;

        parent_class =(GObjectClass *)g_type_class_peek_parent(klass);
        object_class =(GObjectClass *)klass;

        object_class->finalize = contentlist_finalize;
}

/** Init callbacks for the interface GtkTreeModel */
static void contentlist_class_implements(GtkTreeModelIface *iface)
{
        iface->get_flags       = contentlist_get_flags;
        iface->get_n_columns   = contentlist_get_n_columns;
        iface->get_column_type = contentlist_get_column_type;
        iface->get_iter        = contentlist_get_iter;
        iface->get_path        = contentlist_get_path;
        iface->get_value       = contentlist_get_value;
        iface->iter_next       = contentlist_iter_next;
        iface->iter_children   = contentlist_iter_children;
        iface->iter_has_child  = contentlist_iter_has_child;
        iface->iter_n_children = contentlist_iter_n_children;
        iface->iter_nth_child  = contentlist_iter_nth_child;
        iface->iter_parent     = contentlist_iter_parent;
}

/* ------------------------- member functions(customlist) */

/**
 * Constructor.
 *
 * This method is called by gobject before contentlist_new
 * set the size and allocate the memory for the rows.
 */
static void contentlist_init(ContentList *contentlist)
{
        contentlist->size = 0;
        contentlist->rows = NULL;

        /* ID for the iter belongigng to this model */
        contentlist->stamp = g_random_int();
}

/**
 * Destructor.
 *
 * This is called by gobject before an object is destroyed.
 */
static void contentlist_finalize(GObject *object)
{
        ContentList *self = CONTENTLIST(object);

        if(self->size>0) {
                ContentListEntry *rows = self->rows;
                ContentListEntry *end = &rows[self->size];
                ContentListEntry *current;
                for(current=rows; current<end; current++) {
                        if(current->name) {
                                g_free(current->name);
                        }
                        if(current->long_name) {
                                g_free(current->long_name);
                        }
                }

                g_free(rows);
        }
        /* now the parent */
       (*parent_class->finalize)(object);
}


/** Describe this model */
static GtkTreeModelFlags contentlist_get_flags(GtkTreeModel *tree_model)
{
        g_return_val_if_fail(IS_CONTENTLIST(tree_model),(GtkTreeModelFlags)0);

        return GTK_TREE_MODEL_LIST_ONLY
                | GTK_TREE_MODEL_ITERS_PERSIST;
}

/** Get the(fixed) number of columns */
static gint contentlist_get_n_columns(GtkTreeModel *tree_model)
{
        g_return_val_if_fail(IS_CONTENTLIST(tree_model), 0);

        return CONTENTLIST_N_COLUMNS;
}

/** Get the type of a particular column */
static GType contentlist_get_column_type(GtkTreeModel *tree_model,
                                         gint          index)
{
        g_return_val_if_fail(IS_CONTENTLIST(tree_model), G_TYPE_INVALID);
        g_return_val_if_fail(index < CONTENTLIST_N_COLUMNS && index >= 0, G_TYPE_INVALID);

        return column_type[index];
}


/** Convert a tree path into an iter structure */
static gboolean contentlist_get_iter(GtkTreeModel *tree_model,
                                     GtkTreeIter  *iter,
                                     GtkTreePath  *path)
{
        ContentList *contentlist;
        gint *indices;
        gint n;
        gint depth;

        g_return_val_if_fail(IS_CONTENTLIST(tree_model), FALSE);
        g_return_val_if_fail(path, FALSE);
        g_return_val_if_fail(iter, FALSE);

        contentlist = CONTENTLIST(tree_model);

        indices = gtk_tree_path_get_indices(path);
        depth = gtk_tree_path_get_depth(path);

        if(depth!=1) {
                return FALSE;
        }

        n = indices[0];

        if( n >= contentlist->size || n < 0 ) {
                return FALSE;
        }

        iter_set_index(contentlist, iter, n);

        return TRUE;
}

/** Convert an iter to a path */
static GtkTreePath * contentlist_get_path(GtkTreeModel *tree_model,
                                          GtkTreeIter  *iter)
{
        GtkTreePath *path;
        ContentList *contentlist;
        int index;

        g_return_val_if_fail(IS_CONTENTLIST(tree_model), NULL);

        contentlist = CONTENTLIST(tree_model);
        g_return_val_if_fail(iter_is_valid(contentlist, iter), NULL);

        index = iter_get_index(contentlist, iter);

        path = gtk_tree_path_new();
        gtk_tree_path_append_index(path, index);

        return path;
}

/** Return the value for a column of a row */
static void contentlist_get_value(GtkTreeModel *tree_model,
                                  GtkTreeIter  *iter,
                                  gint          column,
                                  GValue       *value)
{
        ContentListEntry *entry;
        int index;
        ContentList *contentlist;

        g_return_if_fail(IS_CONTENTLIST(tree_model));
        g_return_if_fail(column < CONTENTLIST_N_COLUMNS && column>=0);

        contentlist = CONTENTLIST(tree_model);
        g_return_if_fail(iter_is_valid(contentlist, iter));

        g_value_init(value, column_type[column]);

        index = iter_get_index(contentlist, iter);
        entry = &contentlist->rows[index];

        switch(column) {
        case CONTENTLIST_COL_DEFINED:
                g_value_set_boolean(value, entry->defined);
                return;

        case CONTENTLIST_COL_NAME:
                if(entry->defined) {
                        g_value_set_string(value, entry->name);
                } else {
                        g_value_set_string(value, UNDEFINED_STRING);
                }
                return;

        case CONTENTLIST_COL_LONG_NAME:
                if(entry->defined) {
                        g_value_set_string(value, entry->long_name);
                } else {
                        g_value_set_string(value, UNDEFINED_STRING);
                }
                return;

        default:
                g_assert_not_reached();
                return;
        }
}

/** Increment the index in an iter */
static gboolean contentlist_iter_next(GtkTreeModel  *tree_model,
                                      GtkTreeIter   *iter)
{
        ContentList *contentlist;
        guint index;

        g_return_val_if_fail(IS_CONTENTLIST(tree_model), FALSE);
        if(iter == NULL) {
                return FALSE;
        }

        contentlist = CONTENTLIST(tree_model);
        g_return_val_if_fail(iter_is_valid(contentlist, iter), FALSE);

        index = iter_get_index(contentlist, iter);
        index++;
        if(index >= contentlist->size) {
                return FALSE;
        }
        iter->user_data=GUINT_TO_POINTER(index);
        return TRUE;
}


/**
 * Get the parent's children.
 *
 * Only the root (parent==NULL) has children...
 */
static gboolean contentlist_iter_children(GtkTreeModel *tree_model,
                                          GtkTreeIter  *iter,
                                          GtkTreeIter  *parent)
{
        ContentList  *contentlist;

        g_return_val_if_fail(IS_CONTENTLIST(tree_model), FALSE);
        g_return_val_if_fail(iter, FALSE);

        contentlist = CONTENTLIST(tree_model);
        g_return_val_if_fail(parent == NULL || iter_is_valid(contentlist, parent), FALSE);

        if(parent) {
                return FALSE;
        }

        iter_set_index(contentlist, iter, 0);
        return TRUE;
}


/** Check whether a given iter has children */
static gboolean contentlist_iter_has_child(GtkTreeModel *tree_model,
                                           GtkTreeIter  *iter)
{
        if(iter) {
                return FALSE;
        } else { /* fake 'root' iter */
                return TRUE;
        }
}


/**
 * Return the number of children.
 *
 * Only the fake root iter NULL has children...
 */
static gint contentlist_iter_n_children(GtkTreeModel *tree_model,
                                        GtkTreeIter  *iter)
{
  ContentList  *contentlist;

  g_return_val_if_fail(IS_CONTENTLIST(tree_model), -1);

  contentlist = CONTENTLIST(tree_model);

  if(iter==NULL)
    return contentlist->size;
  return 0;
}

/** Get an iter on n-th child */
static gboolean contentlist_iter_nth_child(GtkTreeModel *tree_model,
                                           GtkTreeIter  *iter,
                                           GtkTreeIter  *parent,
                                           gint          n)
{
        ContentList    *contentlist;

        g_return_val_if_fail(IS_CONTENTLIST(tree_model), FALSE);
        g_return_val_if_fail(iter, FALSE);

        contentlist = CONTENTLIST(tree_model);

        if(parent) {
                return FALSE;
        }

        if( n<0 || n >= contentlist->size ) {
                return FALSE;
        }

        iter_set_index(contentlist, iter, n);
        return TRUE;
}

/**
 * Get an iter on the parent node.
 * Iter have no parents...
 */
static gboolean contentlist_iter_parent(GtkTreeModel *tree_model,
                                        GtkTreeIter  *iter,
                                        GtkTreeIter  *child)
{
  return FALSE;
}

/* ------------------------- static functions */

/**
 * Check an iter and make sure it's valid for the given content list
 * @param list
 * @param iter iter to check
 */
static gboolean iter_is_valid(ContentList *list, GtkTreeIter *iter)
{
        guint index;

        if(list==NULL || iter==NULL) {
                return FALSE;
        }
        if(list->stamp!=iter->stamp) {
                return FALSE;
        }
        index = GPOINTER_TO_UINT(iter->user_data);
        if(index>=list->size) {
                return FALSE;
        }
        return TRUE;
}

/**
 * Get the value of an iterator.
 * You should check the iterator first with iter_is_valid()
 * @param list
 * @param iter
 * @param an index 0<=index<list->size
 */
static guint iter_get_index(ContentList *list, GtkTreeIter *iter)
{
        guint index;
        g_return_val_if_fail(list, 0);
        g_return_val_if_fail(iter, 0);

        index = GPOINTER_TO_UINT(iter->user_data);
        g_return_val_if_fail(index<=list->size, 0);
        return index;
}

/**
 * Define an iterator.
 * @param list
 * @param iter iterator to set
 * #param index index of the entry this iterator points to
 */
static void iter_set_index(ContentList *list, GtkTreeIter *iter, guint index)
{
        g_return_if_fail(list);
        g_return_if_fail(iter);
        g_return_if_fail(index<list->size);
        iter->stamp=list->stamp;
        iter->user_data = GUINT_TO_POINTER(index);
}

static void send_row_changed(ContentList *list, guint index)
{
        GtkTreePath *path;
        GtkTreeIter iter;

        iter_set_index(list, &iter, index);
        path = contentlist_get_path(GTK_TREE_MODEL(list), &iter);
        gtk_tree_model_row_changed(GTK_TREE_MODEL(list), path, &iter);
        gtk_tree_path_free(path);
}
