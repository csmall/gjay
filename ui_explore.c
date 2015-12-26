/*
 * Gjay - Gtk+ DJ music playlist creator
 * Copyright (C) 2002-2004 Chuck Groom
 * Copyright (C) 2010-2015 Craig Small 
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2, or (at
 * your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, see <http://www.gnu.org/licenses/>.
 */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */

#include <sys/types.h>
#include <dirent.h>
#include <stdlib.h>
#include <ftw.h> 
#include <string.h>
#include <gtk/gtk.h>
#include "gjay.h"
#include "ipc.h"
#include "ui.h"
#include "ui_private.h"
#include "ui_explore.h"
#include "i18n.h"

enum
{
    NAME_COLUMN = 0,
    IMAGE_COLUMN,
    N_COLUMNS
};

typedef struct {
    int (*fn)( const char *file, 
               const struct stat *sb, int flag,
               GjayExplore *explore,
               gboolean a, gboolean b, gboolean c);
    GjayExplore *explore;
    gboolean extension_filter;
    gboolean flac_supported;
    gboolean ogg_supported;
} ftw_data;

typedef struct {
    gchar    * fname;
    gboolean   is_file;  /* If FALSE, a directory */
} file_to_add;


static int file_iter_depth ( GjayExplore *explore, char * file ) {
    GtkTreeIter *iter;
    GtkTreeIter child, parent;
    int depth;

    iter = g_hash_table_lookup(explore->name_iter_hash, file);
    if (!iter) 
        return -1;
    memcpy(&child, iter, sizeof(GtkTreeIter));
    for (depth = 0; 
         gtk_tree_model_iter_parent(GTK_TREE_MODEL (explore->store), &parent, &child); 
         depth++) {
        memcpy(&child, &parent, sizeof(GtkTreeIter));
    }
    return depth;
}

static gint
compare_name(GtkTreeModel *model,
                    GtkTreeIter *a,
                    GtkTreeIter *b,
                    gpointer user_data)
{
    gchar *a_str, *b_str;
    gint result;

    gtk_tree_model_get(model, a, NAME_COLUMN, &a_str, -1);
    gtk_tree_model_get(model, b, NAME_COLUMN, &b_str, -1);
    result = g_strcmp0(a_str, b_str);
    g_free(a_str);
    g_free(b_str);
    return result;
}

static int tree_walk ( const char *file, 
                      const struct stat *sb, 
                      int flag,
                      GjayExplore *explore,
   						const gboolean extension_filter,
						const gboolean flac_supported,
						const gboolean ogg_supported	) {
    file_to_add * fta;
    int len;

    if ((flag != FTW_F) && (flag != FTW_D))
        return 0;
    
    if ((flag == FTW_F) && (extension_filter)) {
        len = strlen(file);
        if (!((strncasecmp(".mp3", file + len - 4, 4) == 0) ||
              (strncasecmp(".wav", file + len - 4, 4) == 0) ||
              (ogg_supported && strncasecmp(".ogg", file + len - 4, 4) == 0) ||
              (flac_supported && strncasecmp(".flac", file + len - 5, 5) ==0)
              )) {
            return 0;
        }
    }

    fta = g_malloc(sizeof(file_to_add));
    fta->is_file = (flag == FTW_F);
    fta->fname = strdup_to_utf8(file);
    g_queue_push_head(explore->files_to_add_queue, fta);
    return 0;
}


static gint compare_str ( gconstpointer a, gconstpointer b) {
    return g_ascii_strcasecmp((gchar *) a, (gchar *) b);
}

static int
get_iter_path (GtkTreeModel *model,
               GtkTreeIter *child,
               char * buffer,
               gboolean start)
{
    GtkTreeIter parent;
    int len, offset = 0;
    char * name;

    if (gtk_tree_model_iter_parent(model, &parent, child)) {
        offset = get_iter_path(model, &parent, buffer, FALSE);
    }

    gtk_tree_model_get (model, child, NAME_COLUMN, &name, -1);
    len = strlen(name);
    memcpy(buffer + offset, name, len); 
    g_free(name);

    buffer[offset + len] = (start ? '\0' : '/');

    return offset + len + 1;
}

/**
 * Get a glist of directories in a directory
 */
static GList*
explore_dirs_in_dir ( GjayExplore *explore, const gchar * dir)
{
    GtkTreeIter *iter, child;
    char buffer[BUFFER_SIZE];
    GList * list = NULL;
    gboolean has_child;

    iter = g_hash_table_lookup(explore->name_iter_hash, dir);
    if (!iter)
        return NULL;

    has_child = gtk_tree_model_iter_children (GTK_TREE_MODEL (explore->store), 
                                              &child, iter);

    if (has_child) {
        do {
            if (gtk_tree_model_iter_has_child(GTK_TREE_MODEL (explore->store), 
                                              &child)) {
                get_iter_path ( GTK_TREE_MODEL (explore->store),
                                &child,
                                buffer, 
                                TRUE); 
                list = g_list_append(list, g_strdup(buffer));
            } 
        } while (gtk_tree_model_iter_next(GTK_TREE_MODEL (explore->store), &child));
    }
    return list;
}

/**
 * Mark all directories containing at least one file which requires
 * rating/color classification
 */ 
static void
mark_new_dirs ( GjayApp *gjay, char * dir ) {
    GList * list;
    gchar * str;
    char buffer[BUFFER_SIZE];
    int len;
    GjayExplore  *explore = gjay->gui->explore_page;

    strncpy(buffer, dir, BUFFER_SIZE);
    len = strlen(buffer);
    if (len && (buffer[len - 1] == '/'))
        buffer[len - 1] = '\0';
    
    if (strcmp(dir, gjay->prefs->song_root_dir) != 0) {
        if (explore_dir_has_new_songs(explore,
                                      gjay->songs->name_hash, dir,
                                      gjay->verbosity)) {
            str = g_strdup(dir);
            gjay->new_song_dirs = g_list_append(gjay->new_song_dirs, str);
            g_hash_table_insert(gjay->new_song_dirs_hash, str, str);
            explore_path_set_icon(explore, dir,
                                  gjay_get_pixbuf(gjay->gui, PM_DIR_CLOSED_NEW));
        }
    }

    for (list = explore_dirs_in_dir(explore, buffer);
         list; list = g_list_next(list)) {
        mark_new_dirs(gjay, list->data);
        g_free(list->data);
    }
    g_list_free(list);
}

/* Get the depth by counting the number of non-terminal '/' in the path */
static int
file_depth ( char * file )
{
    int len, kk, depth;

    len = strlen(file);
    if (len)
        len--; // Avoid ending '/'
    for (kk = 0, depth = 0; kk < len; kk++) {
        if (file[kk] == '/')
            depth++;
    }
    return depth;
}

static int tree_add_idle (gpointer data) {
    file_to_add * fta;
    GtkTreeIter * parent, * current;
    int pm_type;
    gchar * display_name,  * str;
    GjaySong * s, * original;
    gboolean is_song;
    song_file_type type;
    GjayApp *gjay=(GjayApp*)data;
    GjayExplore *explore = gjay->gui->explore_page;

    if (g_queue_is_empty(explore->files_to_add_queue)) {
        /* We are done with analysis! */
        FILE  * f;
        GList * ll;
        gchar buffer[BUFFER_SIZE];

        /* Create a temporary file for storing any file names requiring
         * analysis */
        snprintf(buffer, BUFFER_SIZE, "%s/%s/%s", 
                 getenv("HOME"), 
                 GJAY_DIR, 
                 GJAY_TEMP);
        f = fopen(buffer, "w");

        /* Sort list of files to analyze  */
        explore->files_to_analyze = g_list_sort (explore->files_to_analyze,
                                                 compare_str);

        for (ll = g_list_first(explore->files_to_analyze); ll; ll = g_list_next(ll)) {
            if (f) 
                fprintf(f, "%s\n", (gchar *) ll->data);
            g_free(ll->data);
        }
        g_list_free(explore->files_to_analyze);
        explore->files_to_analyze = NULL;

        /* Send any new names to the daemon. */
        if (f) {
            fclose(f);
            send_ipc_text(gjay->ipc->ui_fifo, QUEUE_FILE, buffer);
        }

        /* If an entire directory contains any songs which have not 
         * been ranked/colored, set its icon to show that its contents
         * are new */
        mark_new_dirs(gjay, gjay->prefs->song_root_dir);

        set_add_files_progress_visible(FALSE);
        set_analysis_progress_visible(TRUE);
        return FALSE;
    }

    fta = (file_to_add *) explore->files_to_add_queue->tail->data;
    display_name = fta->fname;
    current = g_malloc(sizeof(GtkTreeIter));

    explore->file_to_add_count++;

    if (fta->is_file) {
        assert(explore->parent_name_stack);
        assert(explore->iter_stack);

        while (strncmp(explore->parent_name_stack->tail->data,
                      fta->fname,
                      strlen(explore->parent_name_stack->tail->data)) != 0) {
            g_queue_pop_tail(explore->parent_name_stack);
            g_queue_pop_tail(explore->iter_stack);
        }
        parent = explore->iter_stack->tail->data;

        s = (GjaySong *) g_hash_table_lookup(gjay->songs->name_hash, fta->fname);

        if (s) {
            set_add_files_progress(NULL, (explore->file_to_add_count * 100) / 
                                   explore->total_files_to_add);
            s->in_tree = TRUE;
            if (!s->no_data) {
                pm_type = PM_FILE_SONG;
            } else {
                pm_type = PM_FILE_PENDING;
                /* Analyze this file if it's the first song of a string of
                 * duplicates */
                if (s == g_hash_table_lookup(gjay->songs->inode_dev_hash, 
                                             &s->inode_dev_hash)) {
                    explore->files_to_analyze = g_list_append(
                        explore->files_to_analyze, strdup_to_latin1(s->path));
                }
            }
        } else if (g_hash_table_lookup(gjay->songs->not_hash, fta->fname)) {
            pm_type = PM_FILE_NOSONG;
        }  else {
            set_add_files_progress(fta->fname, 
                                   (explore->file_to_add_count * 100) / 
                                   explore->total_files_to_add);
            gjay->songs->dirty = TRUE;

            s = create_song();
            file_info(gjay->verbosity,
				      gjay->ogg_supported, gjay->flac_supported,
				      fta->fname,
                      &is_song,
                      &s->inode,
                      &s->dev,
                      &s->length,
                      &s->title,
                      &s->artist,
                      &s->album,
                      &type);
            hash_inode_dev(s, TRUE);
            if (is_song) {
                song_set_path(s, fta->fname);
                /* Check for symlinkery */
                original = g_hash_table_lookup(gjay->songs->inode_dev_hash, 
                                               &s->inode_dev_hash);
                if (original) { 
                    song_set_repeats(s, original);
                    pm_type = PM_FILE_SONG;
                } else { 
                    g_hash_table_insert(gjay->songs->inode_dev_hash, 
                                        &s->inode_dev_hash, s);
                    /* Add to analysis queue */
                    explore->files_to_analyze = g_list_append(explore->files_to_analyze,
                                                     strdup_to_latin1(fta->fname));
                    pm_type = PM_FILE_PENDING;
                }
                gjay->songs->songs = g_list_append(gjay->songs->songs, s);
                g_hash_table_insert(gjay->songs->name_hash, s->path, s);
                s->in_tree = TRUE;
            } else {
                delete_song(s);
                str = g_strdup(fta->fname);
                gjay->songs->not_songs = g_list_append(gjay->songs->not_songs, str);
                g_hash_table_insert(gjay->songs->not_hash, str, (gpointer) TRUE);
                pm_type = PM_FILE_NOSONG;
            }
        }
        display_name = fta->fname + 1 + strlen(explore->parent_name_stack->tail->data);

        gtk_tree_store_append (explore->store, current, parent);
        gtk_tree_store_set (explore->store, current,
                            NAME_COLUMN, display_name,
                            IMAGE_COLUMN, gjay->gui->pixbufs[pm_type],
                            -1);

        str = strdup(fta->fname);
        explore->file_name_in_tree = g_list_append(explore->file_name_in_tree, str);
        g_hash_table_insert ( explore->name_iter_hash,
                              str,
                              current); 
    } else {
        /* Directory */
        if (g_queue_is_empty(explore->iter_stack)) {
            parent = NULL;
        } else {
            assert(explore->parent_name_stack->tail);
            while (file_depth((char *) fta->fname) <= 
                   file_depth((char *) explore->parent_name_stack->tail->data)) {
                g_queue_pop_tail(explore->parent_name_stack);
                g_queue_pop_tail(explore->iter_stack);
            }
            parent = explore->iter_stack->tail->data;  
            display_name = fta->fname + 1 + strlen(explore->parent_name_stack->tail->data);
        }

        gtk_tree_store_append (explore->store, current, parent);
        gtk_tree_store_set (explore->store, current,
                            NAME_COLUMN, display_name,
                            IMAGE_COLUMN,
                            gjay_get_pixbuf(gjay->gui, PM_DIR_CLOSED),
                            -1);
        str = strdup(fta->fname);
        explore->file_name_in_tree = g_list_append(explore->file_name_in_tree, str);
        g_hash_table_insert ( explore->name_iter_hash,
                              str,
                              current);

        g_queue_push_tail(explore->iter_stack, current);
        g_queue_push_tail(explore->parent_name_stack, str);
        gjay->tree_depth = MAX(gjay->tree_depth,
                               g_list_length(explore->parent_name_stack->head));
    }

    g_queue_pop_tail(explore->files_to_add_queue);
    g_free(fta->fname);
    g_free(fta);

    return TRUE;
}


/**
 * The user has selected a file in the tree. Figure out what file it is
 * (the filename) and ship it off to the selection pane 
 */
static void
select_row (GtkTreeSelection *selection, gpointer data)
{
    char buffer[BUFFER_SIZE];
    GtkTreeIter iter;
    GtkTreeModel *model;
    gchar * name;
    gboolean has_child;
    GjayApp *gjay=(GjayApp*)data;
    struct GjayExplore *explore=gjay->gui->explore_page;

    if (gtk_tree_selection_get_selected (selection, &model, &iter)) {
        /* Very special and quite annoying case. If we have changed root
           directories, it is possible this method gets called before the
           tree is setup and the root directory gets called as if it were
           a normal file because it doesn't have any children. */
        if (!g_queue_is_empty(explore->files_to_add_queue) && 
            (gtk_tree_store_iter_depth(explore->store, &iter) == 0)) {
            return;
        }

        get_iter_path(model, &iter, buffer, TRUE);
        gtk_tree_model_get (model, &iter, NAME_COLUMN, &name, -1);
        has_child = gtk_tree_model_iter_has_child(model, &iter);
        if (has_child) {
            set_selected_file(gjay, buffer, name, TRUE);
        } else {
//if (!has_child && g_hash_table_lookup(song_name_hash, buffer)) {
            set_selected_file(gjay, buffer, name, FALSE);
        }
        g_free(name);
    }
}

static GtkWidget*
create_dir_panel(GjayApp *gjay, struct GjayExplore *explore)
{
    GtkCellRenderer *renderer;
    GtkTreeViewColumn *column;
    GtkTreeSelection *selection;
    GtkWidget *frame, *swin;

    frame = gtk_frame_new(NULL);
    gtk_frame_set_shadow_type(GTK_FRAME(frame), GTK_SHADOW_IN);

    swin = gtk_scrolled_window_new (NULL, NULL);
    gtk_widget_set_usize(swin, APP_WIDTH * 0.5, -1);
    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (swin),
                                    GTK_POLICY_AUTOMATIC,
                                    GTK_POLICY_AUTOMATIC);
    gtk_container_add(GTK_CONTAINER(frame), GTK_WIDGET(swin));

    explore->store = gtk_tree_store_new (N_COLUMNS, G_TYPE_STRING, GDK_TYPE_PIXBUF);
    gtk_tree_sortable_set_sort_func(GTK_TREE_SORTABLE(explore->store),
                                    NAME_COLUMN,
                                    compare_name,
                                    NULL,
                                    NULL);
    gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE(explore->store), 
                                          NAME_COLUMN,
                                          GTK_SORT_ASCENDING);

    explore->tree_view = gtk_tree_view_new_with_model(
                                GTK_TREE_MODEL(explore->store));
    selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (explore->tree_view));
    gtk_tree_selection_set_mode (selection, GTK_SELECTION_SINGLE);
    g_signal_connect (G_OBJECT (selection), 
                      "changed",
                      G_CALLBACK (select_row),
                      gjay);

    renderer= gtk_cell_renderer_pixbuf_new();
    column = gtk_tree_view_column_new_with_attributes ("Image", renderer,
                                                       "pixbuf", IMAGE_COLUMN,
                                                       NULL);
    gtk_tree_view_append_column (GTK_TREE_VIEW (explore->tree_view), column);

    renderer = gtk_cell_renderer_text_new ();
    column = gtk_tree_view_column_new_with_attributes ("Filename", renderer,
                                                       "text", NAME_COLUMN,
                                                       NULL);
    gtk_tree_view_append_column (GTK_TREE_VIEW (explore->tree_view), column);
    gtk_tree_view_set_headers_visible(GTK_TREE_VIEW (explore->tree_view),
                                      FALSE);

    gtk_container_add(GTK_CONTAINER(swin), GTK_WIDGET(explore->tree_view));


    return frame;
}

static GtkWidget *
create_selection_none(GjayApp *gjay, struct GjayExplore *explore)
{
    GtkWidget *frame, *label;

    frame = gtk_frame_new(NULL);
    gtk_frame_set_shadow_type(GTK_FRAME(frame), GTK_SHADOW_IN);

    label = gtk_label_new(_("Nothing Selected"));
    gtk_container_add(GTK_CONTAINER(frame), label);
    return frame;

}

static GtkWidget *
create_selection_dir(GjayApp *gjay, struct GjayExplore *explore)
{
    GtkWidget *frame, *label;

    frame = gtk_frame_new(NULL);
    gtk_frame_set_shadow_type(GTK_FRAME(frame), GTK_SHADOW_IN);

    label = gtk_label_new(_("directory Selected"));
    gtk_container_add(GTK_CONTAINER(frame), label);
    return frame;

}

static GtkWidget *
create_selection_file(GjayApp *gjay, struct GjayExplore *explore)
{
    GtkWidget *frame, *label;

    frame = gtk_frame_new(NULL);
    gtk_frame_set_shadow_type(GTK_FRAME(frame), GTK_SHADOW_IN);

    label = gtk_label_new(_("file Selected"));
    gtk_container_add(GTK_CONTAINER(frame), label);
    return frame;

}

struct GjayExplore
*explore_new(GjayApp *gjay)
{
    struct GjayExplore *explore;
    GtkWidget *dir_frame;

    explore = g_malloc0(sizeof(struct GjayExplore));
    explore->current_selection = EXPLORE_NOSEL;
    explore->widget = gtk_hpaned_new();

    dir_frame = create_dir_panel(gjay, explore);
    gtk_paned_pack1(GTK_PANED(explore->widget), dir_frame, TRUE, TRUE);

    explore->selections[EXPLORE_NOSEL] = create_selection_none(gjay, explore);
    explore->selections[EXPLORE_DIR] = create_selection_dir(gjay, explore);
    explore->selections[EXPLORE_FILE] = create_selection_file(gjay, explore);

    explore->files_to_add_queue = g_queue_new();
    return explore;
}

void
explore_show(GjayApp *gjay)
{
    GjayExplore *explore = gjay->gui->explore_page;
    gtk_paned_pack2(GTK_PANED(explore->widget),
                    explore->selections[explore->current_selection], TRUE, TRUE);
    printf("show explore page\n");
}

void
explore_destroy(struct GjayExplore *explore)
{
    gtk_widget_destroy(explore->widget);
    g_free(explore);
}

static int gjay_ftw(const gchar *dir, const guint depth, ftw_data *fdata) {
    int retval = 0, flag, dir_len;
    struct stat st;
    gchar *buffer;
    const gchar *dir_ent;
    GDir *gdir;

    /* Sanity check */
    if (depth == 0)
        return 0;

    /* Call fn on this directory */
    buffer = g_strdup(dir);
    dir_len = strlen(dir);
    if (dir_len > 0 && (buffer[dir_len - 1] == '/'))
        buffer[dir_len - 1] = '\0';
    if (stat(buffer, &st) == 0)
        fdata->fn(buffer, &st, FTW_D, fdata->explore,
                  fdata->extension_filter,
                  fdata->flac_supported, fdata->ogg_supported);
    else {
            g_free(buffer);
            return -1;
    }
    g_free(buffer);

    if ( (gdir = g_dir_open(dir, 0, NULL)) == NULL)
        return -1;

    while (retval==0){
        if ( (dir_ent = g_dir_read_name(gdir)) == NULL)
            break;
        flag=0;
        buffer = g_strdup_printf("%s%s%s", dir,
                                 (dir_len && (dir[dir_len - 1] == '/')) ? "" : "/",
                                 dir_ent);
        if (stat(buffer, &st) == 0) {
            if (S_ISDIR(st.st_mode))
                flag = FTW_D;
            else if (S_ISREG(st.st_mode))
                flag = FTW_F;
            else
                flag = FTW_NS;
            if (flag & FTW_D) {
                retval = gjay_ftw(buffer, depth-1, fdata);
            } else {
                retval = fdata->fn(
                                   buffer, &st, flag,
                                   fdata->explore,
                                 fdata->extension_filter,
                                fdata->flac_supported, fdata->ogg_supported);
            }
        }
        g_free(buffer);
    }
    g_dir_close(gdir);
    return retval;
}

/** 
 * Change the root of the user-visible directory tree. We will not destroy
 * old song information, but only mark those songs which are visible in the
 * tree.
 * 
 * Note that root_dir is UTF8 encoded
 */
void explore_set_root (GjayApp *gjay) {
    GList * llist=NULL;
    char buffer[BUFFER_SIZE];
	ftw_data fdata;
    GjayExplore *explore = gjay->gui->explore_page;

    if (!gjay->prefs->song_root_dir)
        return;

    gtk_tree_store_clear(explore->store);
    gjay->tree_depth = 0;

    /* Unmark current songs */
    for (llist = g_list_first(gjay->songs->songs); llist!=NULL; llist = g_list_next(llist)) {
        SONG(llist)->in_tree = FALSE;
    }

    /* Clear queue of files which were pending addition from previous
     * tree-building attempt. This should rarely be needed. */
    if (explore->files_to_add_queue) {
        while (!g_queue_is_empty(explore->files_to_add_queue)) {
            g_free(explore->files_to_add_queue->tail->data);
            g_queue_pop_tail(explore->files_to_add_queue);
        }
    } else {
        explore->files_to_add_queue  = g_queue_new();
    }

    for (llist = gjay->new_song_dirs; llist; llist = g_list_next(llist))
        g_free(llist->data);
    g_list_free(gjay->new_song_dirs);
    gjay->new_song_dirs = NULL;
    if (gjay->new_song_dirs_hash)
        g_hash_table_destroy(gjay->new_song_dirs_hash);
    gjay->new_song_dirs_hash = g_hash_table_new(g_str_hash, g_str_equal);

    if (explore->name_iter_hash)
        g_hash_table_destroy(explore->name_iter_hash);
    explore->name_iter_hash = g_hash_table_new(g_str_hash, g_str_equal);

    for (llist = g_list_first(explore->file_name_in_tree); 
         llist;
         llist = g_list_next(llist)) {
        g_free(llist->data);
    }
    g_list_free(explore->file_name_in_tree);
    explore->file_name_in_tree = NULL;

    /* Check to see if the directory exists */
    if (access(gjay->prefs->song_root_dir, R_OK | X_OK)) {
        snprintf(buffer, BUFFER_SIZE, "Cannot acces the base directory '%s'! Maybe it moved? You can pick another directory in the prefs", gjay->prefs->song_root_dir);
        g_warning("%s", buffer);
        return;
    }
    if (explore->iter_stack)
        g_queue_free (explore->iter_stack);
    explore->iter_stack = g_queue_new();

    if (explore->parent_name_stack)
        g_queue_free(explore->parent_name_stack);
    explore->parent_name_stack = g_queue_new();

    if (explore->files_to_analyze)
        g_list_free(explore->files_to_analyze);
    explore->files_to_analyze = NULL;

    /* Recurse through the directory tree, adding file names to 
     * a stack. In spare cycles, we'll process these files properly for
     * list display and requesting daemon processing */
    fdata.fn = tree_walk;
    fdata.explore = explore;
    fdata.extension_filter = gjay->prefs->extension_filter;
    fdata.flac_supported = gjay->flac_supported;
    fdata.ogg_supported = gjay->ogg_supported;

    gjay_ftw(gjay->prefs->song_root_dir, 10, &fdata);
    gtk_idle_add(tree_add_idle, gjay);

    explore->total_files_to_add = g_list_length(explore->files_to_add_queue->head);
    explore->file_to_add_count = 0;

    set_add_files_progress("Scanning tree...", 0);
    set_analysis_progress_visible(FALSE);
    set_add_files_progress_visible(TRUE);

    /* Run idle loop once to force display of root dir */
    tree_add_idle(gjay);
}

/**
 * Redraw the icon next to the corresponding file in the tree. Return TRUE
 * if file was found in tree, FALSE if not.
 */
gboolean explore_path_set_icon (struct GjayExplore *explore,
                                const char *path,
                                GdkPixbuf *pixbuf)
{
    GtkTreeIter  * iter;

    if (!explore->name_iter_hash)
        return FALSE;

    iter = g_hash_table_lookup(explore->name_iter_hash, path);
    if (iter) {
        gtk_tree_store_set (explore->store,
                            iter,
                            IMAGE_COLUMN,
                            pixbuf,
                            -1);
        return TRUE;
    }
    return FALSE;
}

void
explore_select_song(GjayExplore *explore, GjaySong *s)
{
    printf("explore select song\n");
}


/**
 * Return TRUE if the directory contains at least one song which
 * has not been rated or color-categorized
 */
gboolean explore_dir_has_new_songs ( GjayExplore *explore,
                                     GHashTable *name_hash,
                                     const gchar * dir,
                                     const guint verbosity )
{
    gboolean result = FALSE;
    GList * list;
    GjaySong * s;

    list = explore_files_in_dir(explore, dir, TRUE);
    for (; list; list = g_list_next(list)) {
        if (result == FALSE) {
            s = g_hash_table_lookup(name_hash, list->data);
            if (s) {
                if (s->no_rating && s->no_color) {
                    result = TRUE;
                }
            } else if (verbosity) {
                fprintf(stderr, "Explore_dir_has_new_songs: missing dir %s\n",
                        dir);
            }
        }
        g_free(list->data);
    }
    g_list_free(list);
    return result;
}

/**
 * Get a glist of files in a directory
 */
GList * explore_files_in_dir ( const GjayExplore *explore,
                               const gchar * dir,
                               const gboolean recursive)
{
    GtkTreeIter *iter, child;
    char buffer[BUFFER_SIZE];
    gboolean has_child;
    GList * list = NULL;

    iter = g_hash_table_lookup(explore->name_iter_hash, dir);
    if (!iter)
        return NULL;
    has_child = gtk_tree_model_iter_children (GTK_TREE_MODEL (explore->store),
                                              &child, iter);

    if (has_child) {
        do {
            if (!gtk_tree_model_iter_has_child(GTK_TREE_MODEL (explore->store), 
                                               &child)) {
                get_iter_path ( GTK_TREE_MODEL (explore->store),
                                &child,
                                buffer, 
                                TRUE);        
                list = g_list_append(list, g_strdup(buffer));
            } else if (recursive) {
                get_iter_path ( GTK_TREE_MODEL (explore->store),
                                &child,
                                buffer, 
                                TRUE); 
                list = g_list_concat(list, explore_files_in_dir(explore, buffer, TRUE));
            }
        } while (gtk_tree_model_iter_next(GTK_TREE_MODEL (explore->store), &child));
    }
    return list;
}


/* How many directory steps separate files 1 and 2? */
gint explore_files_depth_distance ( GjayExplore *explore,
                                    char * file1, 
                                    char * file2 ) {
    char buffer[BUFFER_SIZE];
    int f1, f2, shared, k, len;

    len = MIN(MIN(strlen(file1), strlen(file2)), BUFFER_SIZE);
    for (k = 0; (k < len) && (file1[k] == file2[k]); k++) 
        buffer[k] = file1[k];
    /* Work backwards to find forward slash in common */
    while(buffer[k] != '/')
        k--;
    /* Replace slash with null termination */
    buffer[k] = '\0';
    f1 = file_iter_depth(explore, file1);
    f2 = file_iter_depth(explore, file2);
    shared = file_iter_depth(explore, buffer) + 1;

    if (f1 && f2 && shared) 
        return ((f1 - shared) + (f2 - shared));
    else
        return -1;
}

static gint
animate_path_icon(gpointer data)
{
    GjayApp *gjay = data;
    GjayExplore *explore = gjay->gui->explore_page;

    explore_path_set_icon(explore, explore->animate_file,
                 gjay_get_pixbuf(gjay->gui, PM_FILE_PENDING+explore->animate_frame));

    explore->animate_frame = (explore->animate_frame+ 1) % 4;
    return TRUE;
}

void
explore_animate_pending (GjayApp *gjay, char * file)
{
    GjayExplore *explore = gjay->gui->explore_page;

    explore_animate_stop(explore);
    explore->animate_file = g_strdup(file);
    explore->animate_timeout = gtk_timeout_add( 250,
                                       animate_path_icon,
                                       gjay);
}

void explore_animate_stop ( GjayExplore *explore ) {
    g_free(explore->animate_file);
    explore->animate_file = NULL;
    explore->animate_frame = 0;
    if (explore->animate_timeout)
        gtk_timeout_remove(explore->animate_timeout);
    explore->animate_timeout = 0;
}


