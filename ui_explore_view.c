/**
 * GJay, copyright (c) 2002 Chuck Groom
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 1, or (at
 * your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
 */

#include <stdlib.h>
#include <ftw.h> 
#include <string.h>
#include <time.h>
#include "gjay.h"
#include "ui.h"
#include "ipc.h"


enum
{
    NAME_COLUMN = 0,
    IMAGE_COLUMN,
    N_COLUMNS
};

typedef struct {
    gchar    * fname;
    gboolean   is_file;  /* If FALSE, a directory */
} file_to_add;


GtkTreeStore * store = NULL;
GQueue       * iter_stack = NULL;
GQueue       * parent_name_stack = NULL; /* Stack of file name (one level
                                            down from parent_stack) */
gint           tree_depth;               /* How deep does the tree go */

/* We hash each file name to the corresponding GtkCTreeNode */
static GHashTable * file_name_iter_hash = NULL;
static GList      * file_name_in_tree = NULL;
static GQueue     * files_to_add_queue = NULL; 
static GList      * files_to_analyze = NULL;

static gint         animate_timeout = 0;
static gint         animate_frame = 0;
static gchar      * animate_file = NULL;
static gint         total_files_to_add, file_to_add_count;


static int    tree_walk       ( const char *file, 
                                const struct stat *sb, 
                                int flag );
static int    tree_add_idle   ( gpointer data );
static void   select_row      ( GtkTreeSelection *selection, 
                                gpointer data);
static int    get_iter_path   ( GtkTreeModel *tree_model,
                                GtkTreeIter *child,
                                char * buffer,
                                gboolean is_start );
static int    file_depth      ( char * file );
static gint   explore_animate ( gpointer data );
static gint   iter_sort_strcmp ( GtkTreeModel *model,
                                 GtkTreeIter *a,
                                 GtkTreeIter *b,
                                 gpointer user_data);
gint          compare_str     ( gconstpointer a,
                                gconstpointer b );

GtkWidget * make_explore_view ( void ) {
    GtkWidget * swin;
    GtkCellRenderer *renderer;
    GtkTreeViewColumn *column;
    GtkTreeSelection *select;
    GtkWidget * tree_view;
    
    store = gtk_tree_store_new (N_COLUMNS, G_TYPE_STRING, GDK_TYPE_PIXBUF);
    gtk_tree_sortable_set_sort_func(GTK_TREE_SORTABLE(store),
                                    NAME_COLUMN,
                                    iter_sort_strcmp,
                                    NULL,
                                    NULL);
    gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE(store), 
                                          NAME_COLUMN,
                                          GTK_SORT_ASCENDING);
    
    tree_view = gtk_tree_view_new_with_model (GTK_TREE_MODEL(store));
    select = gtk_tree_view_get_selection (GTK_TREE_VIEW (tree_view));
    gtk_tree_selection_set_mode (select, GTK_SELECTION_SINGLE);
    g_signal_connect (G_OBJECT (select), 
                      "changed",
                      G_CALLBACK (select_row),
                      NULL);
    
    renderer= gtk_cell_renderer_pixbuf_new();
    column = gtk_tree_view_column_new_with_attributes ("Image", renderer,
                                                       "pixbuf", IMAGE_COLUMN,
                                                       
NULL);
    gtk_tree_view_append_column (GTK_TREE_VIEW (tree_view), column);

    renderer = gtk_cell_renderer_text_new ();
    column = gtk_tree_view_column_new_with_attributes ("Filename", renderer,
                                                       "text", NAME_COLUMN,
                                                       NULL);
    gtk_tree_view_append_column (GTK_TREE_VIEW (tree_view), column);
    gtk_tree_view_set_headers_visible(GTK_TREE_VIEW (tree_view), FALSE);

    swin = gtk_scrolled_window_new (NULL, NULL);
    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (swin),
                                    GTK_POLICY_AUTOMATIC,
                                    GTK_POLICY_AUTOMATIC);
    gtk_container_add(GTK_CONTAINER(swin), GTK_WIDGET(tree_view));
    gtk_widget_set_usize(swin, APP_WIDTH * 0.5, -1);
    return swin;
}


/** 
 * Change the root of the user-visible directory tree. We will not destroy
 * old song information, but only mark those songs which are visible in the
 * tree.
 * 
 * Note that root_dir is UTF8 encoded
 */
void explore_view_set_root ( char * root_dir ) {
    GList * llist;
    char buffer[BUFFER_SIZE];

    if (!root_dir)
        return;

    gtk_tree_store_clear(store);
    tree_depth = 0;
    
    /* Unmark current songs */
    for (llist = g_list_first(songs); llist; llist = g_list_next(llist)) {
        SONG(llist)->in_tree = FALSE;
    }
    
    /* Clear queue of files which were pending addition from previous
     * tree-building attempt. This should rarely be needed. */
    if (files_to_add_queue) {
        while (!g_queue_is_empty(files_to_add_queue)) {
            g_free(files_to_add_queue->tail->data);
            g_queue_pop_tail(files_to_add_queue);
        }
    } else {
        files_to_add_queue  = g_queue_new();
    }
    
    if (file_name_iter_hash)
        g_hash_table_destroy(file_name_iter_hash);
    file_name_iter_hash = g_hash_table_new(g_str_hash, g_str_equal);

    for (llist = g_list_first(file_name_in_tree); 
         llist;
         llist = g_list_next(llist)) {
        g_free(llist->data);
    }
    g_list_free(file_name_in_tree);
    file_name_in_tree = NULL;

    /* Check to see if the directory exists */
    if (access(root_dir, R_OK | X_OK)) {
        snprintf(buffer, BUFFER_SIZE, "Cannot acces the base directory '%s'! Maybe it moved? You can pick another directory in the prefs", root_dir);
        display_message(buffer);
        return;
    }    
    if (iter_stack)
        g_queue_free (iter_stack); 
    iter_stack = g_queue_new(); 
    
    if (parent_name_stack) 
        g_queue_free(parent_name_stack);
    parent_name_stack = g_queue_new();

    if (files_to_analyze)
        g_list_free(files_to_analyze);
    files_to_analyze = NULL;

    /* Recurse through the directory tree, adding file names to 
     * a stack. In spare cycles, we'll process these files properly for
     * list display and requesting daemon processing */
    ftw(root_dir, tree_walk, 10);
    
    gtk_idle_add(tree_add_idle, NULL);

    total_files_to_add = g_list_length(files_to_add_queue->head);
    file_to_add_count = 0;
    
    set_add_files_progress("Scanning tree...", 0);
    set_analysis_progress_visible(FALSE);
    set_add_files_progress_visible(TRUE);
    
    /* Run idle loop once to force display of root dir */
    tree_add_idle(NULL);
}


gint explore_view_set_root_idle ( gpointer data ) {
    explore_view_set_root((gchar *) data);
    if (gtk_notebook_get_current_page(GTK_NOTEBOOK(notebook)) !=
        TAB_EXPLORE) {
        gtk_notebook_set_current_page(GTK_NOTEBOOK(notebook), TAB_EXPLORE);
    } else {
        switch_page(GTK_NOTEBOOK(notebook),
                    NULL,
                    TAB_EXPLORE,
                    NULL);
    }
    return FALSE;
}



static int tree_walk (const char *file, 
                      const struct stat *sb, 
                      int flag ) {
    file_to_add * fta;
    int len;
    
    if ((flag != FTW_F) && (flag != FTW_D))
        return 0;
    
    if ((flag == FTW_F) && (prefs.extension_filter)) {
        len = strlen(file);
        if (!((strncasecmp(".mp3", file + len - 4, 4) == 0) ||
              (strncasecmp(".ogg", file + len - 4, 4) == 0) ||
              (strncasecmp(".wav", file + len - 4, 4) == 0))) {
            return 0;
        }
    }

    fta = g_malloc(sizeof(file_to_add));
    fta->is_file = (flag == FTW_F);
    fta->fname = strdup_to_utf8(file);
    g_queue_push_head(files_to_add_queue, fta);
    return 0;
}




static int tree_add_idle (gpointer data) {
    file_to_add * fta;
    GtkTreeIter * parent, * current;
    int pm_type;
    gchar * display_name,  * str;
    song * s, * original;
    gboolean is_song;
    song_file_type type;
    
    if (g_queue_is_empty(files_to_add_queue)) {
        /* We are done with analysis!*/
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
        files_to_analyze = g_list_sort (files_to_analyze,  compare_str);

        for (ll = g_list_first(files_to_analyze); ll; ll = g_list_next(ll)) {
            if (f) 
                fprintf(f, "%s\n", (gchar *) ll->data);
            g_free(ll->data);
        }
        g_list_free(files_to_analyze);
        files_to_analyze = NULL;
        
        /* Send any new names to the daemon. */
        if (f) {
            fclose(f);
            send_ipc_text(ui_pipe_fd, QUEUE_FILE, buffer);
        } 
        set_add_files_progress_visible(FALSE);
        set_analysis_progress_visible(TRUE);
        return FALSE;
    }

    fta = (file_to_add *) files_to_add_queue->tail->data;
    display_name = fta->fname;
    current = g_malloc(sizeof(GtkTreeIter));

    file_to_add_count++;

    if (fta->is_file) {
        assert(parent_name_stack);
        assert(iter_stack);

        while (strncmp(parent_name_stack->tail->data,
                       fta->fname,
                       strlen(parent_name_stack->tail->data)) != 0) {
            g_queue_pop_tail(parent_name_stack);
            g_queue_pop_tail(iter_stack);
        }
        parent = iter_stack->tail->data;   

        s = (song *) g_hash_table_lookup(song_name_hash, fta->fname);
        
        if (s) {
            set_add_files_progress(NULL, (file_to_add_count * 100) / 
                                   total_files_to_add);
            s->in_tree = TRUE;

            if (!s->no_data) {
                pm_type = PM_FILE_SONG;
            } else {
                pm_type = PM_FILE_PENDING;
                /* Analyze this file if it's the first song of a string of
                 * duplicates */
                if (s == g_hash_table_lookup(song_inode_hash, 
                                             &s->inode)) {
                    files_to_analyze = g_list_append(
                        files_to_analyze, strdup_to_latin1(s->path));
                }
            }
        } else if (g_hash_table_lookup(not_song_hash, fta->fname)) {
            pm_type = PM_FILE_NOSONG;
        }  else {
            set_add_files_progress(fta->fname, 
                                   (file_to_add_count * 100) / 
                                   total_files_to_add);
            songs_dirty = TRUE;

            s = create_song();
            file_info(fta->fname,
                      &is_song,
                      &s->inode,
                      &s->length,
                      &s->title,
                      &s->artist,
                      &s->album,
                      &type);
            if (is_song) {
                song_set_path(s, fta->fname);
                /* Check for symlinkery */
                original = g_hash_table_lookup(song_inode_hash, 
                                               &s->inode);
                if (original) { 
                    song_set_repeats(s, original);
                    pm_type = PM_FILE_SONG;
                } else { 
                    g_hash_table_insert(song_inode_hash, 
                                        &s->inode, s);
                    /* Add to analysis queue */
                    files_to_analyze = g_list_append(files_to_analyze,
                                                     strdup_to_latin1(fta->fname));
                    pm_type = PM_FILE_PENDING;
                }
                songs = g_list_append(songs, s);
                g_hash_table_insert(song_name_hash, s->path, s);
                s->in_tree = TRUE;
            } else {
                delete_song(s);
                str = g_strdup(fta->fname);
                not_songs = g_list_append(not_songs, str);
                g_hash_table_insert(not_song_hash, str, (gpointer) TRUE);
                pm_type = PM_FILE_NOSONG;
            }
        }
        display_name = fta->fname + 1 + strlen(parent_name_stack->tail->data);

        gtk_tree_store_append (store, current, parent);
        gtk_tree_store_set (store, current,
                            NAME_COLUMN, display_name,
                            IMAGE_COLUMN, pixbufs[pm_type],
                            -1);
        
        str = strdup(fta->fname);
        file_name_in_tree = g_list_append(file_name_in_tree, str);
        g_hash_table_insert ( file_name_iter_hash,
                              str,
                              current); 
    } else {
        /* Directory */
        if (g_queue_is_empty(iter_stack)) {
            parent = NULL;
        } else {
            assert(parent_name_stack->tail);
            while (strncmp(parent_name_stack->tail->data,
                           fta->fname,
                           strlen(parent_name_stack->tail->data)) != 0) {
                g_queue_pop_tail(parent_name_stack);
                g_queue_pop_tail(iter_stack);
            }
            parent = iter_stack->tail->data;  
            display_name = fta->fname + 1 + strlen(parent_name_stack->tail->data);
        }
        
        gtk_tree_store_append (store, current, parent);
        gtk_tree_store_set (store, current,
                            NAME_COLUMN, display_name,
                            IMAGE_COLUMN, pixbufs[PM_DIR_CLOSED],
                            -1);
        str = strdup(fta->fname);
        file_name_in_tree = g_list_append(file_name_in_tree, str);
        g_hash_table_insert ( file_name_iter_hash,
                              str,
                              current);
        
        g_queue_push_tail(iter_stack, current);
        g_queue_push_tail(parent_name_stack, str);
        tree_depth = MAX(tree_depth, g_list_length(parent_name_stack->head));
    }
    
    g_queue_pop_tail(files_to_add_queue);
    g_free(fta->fname);
    g_free(fta);

    return TRUE;
}


static int get_iter_path (GtkTreeModel *model,
                          GtkTreeIter *child,
                          char * buffer,
                          gboolean start) {
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
 * The user has selected a file in the tree. Figure out what file it is
 * (the filename) and ship it off to the selection pane 
 */
static void select_row (GtkTreeSelection *selection, gpointer data) {
    char buffer[BUFFER_SIZE];
    GtkTreeIter iter;
    GtkTreeModel *model;
    gchar * name;
    
    if (gtk_tree_selection_get_selected (selection, &model, &iter)) {
        /* Very special and quite annoying case. If we have changed root
           directories, it is possible this method gets called before the
           tree is setup and the root directory gets called as if it were
           a normal file because it doesn't have any children. */
        if (!g_queue_is_empty(files_to_add_queue) && 
            (gtk_tree_store_iter_depth(store, &iter) == 0)) {
            return;
        }

        get_iter_path(model, &iter, buffer, TRUE);
        gtk_tree_model_get (model, &iter, NAME_COLUMN, &name, -1);
        set_selected_file(buffer, name, 
                          gtk_tree_model_iter_has_child(model, &iter));
        g_free(name);
    }
}



/**
 * Redraw the icon next to the corresponding file in the tree. Return TRUE
 * if file was found in tree, FALSE if not.
 */
gboolean explore_update_file_pm ( char * file, int type ) {
    GtkTreeIter  * iter;

    if (!file_name_iter_hash)
        return FALSE;
    
    iter = g_hash_table_lookup(file_name_iter_hash, file);
    if (iter) {
        gtk_tree_store_set (store, 
                            iter,
                            IMAGE_COLUMN,
                            pixbufs[type], 
                            -1);
        return TRUE;
    } 
    return FALSE;
}



/**
 * Get a glist of files in a directory
 */
GList * explore_files_in_dir ( char * dir, gboolean recursive) {
    GtkTreeIter *iter, child;
    char buffer[BUFFER_SIZE];
    gboolean has_child;
    GList * list = NULL;

    iter = g_hash_table_lookup(file_name_iter_hash, dir);
    if (!iter)
        return NULL;
    has_child = gtk_tree_model_iter_children (GTK_TREE_MODEL (store), 
                                              &child, iter);
    
    if (has_child) {
        do {
            if (!gtk_tree_model_iter_has_child(GTK_TREE_MODEL (store), 
                                               &child)) {
                get_iter_path ( GTK_TREE_MODEL (store),
                                &child,
                                buffer, 
                                TRUE);        
                list = g_list_append(list, g_strdup(buffer));
            } else if (recursive) {
                get_iter_path ( GTK_TREE_MODEL (store),
                                &child,
                                buffer, 
                                TRUE); 
                list = g_list_concat(list, explore_files_in_dir(buffer, TRUE));
            }
        } while (gtk_tree_model_iter_next(GTK_TREE_MODEL (store), &child));
    }
    return list;
}


static int file_depth ( char * file ) {
    GtkTreeIter *iter;
    GtkTreeIter child, parent;
    int depth;

    iter = g_hash_table_lookup(file_name_iter_hash, file);
    if (!iter) 
        return -1;
    memcpy(&child, iter, sizeof(GtkTreeIter));
    for (depth = 0; 
         gtk_tree_model_iter_parent(GTK_TREE_MODEL (store), &parent, &child); 
         depth++) {
        memcpy(&child, &parent, sizeof(GtkTreeIter));
    }
    return depth;
}



/* How many directory steps separate files 1 and 2? */
gint explore_files_depth_distance ( char * file1, 
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
    f1 = file_depth(file1);
    f2 = file_depth(file2);
    shared = file_depth(buffer) + 1;

    if (f1 && f2 && shared) 
        return ((f1 - shared) + (f2 - shared));
    else
        return -1;
}


void explore_animate_pending ( char * file ) {
    explore_animate_stop();
    animate_file = g_strdup(file);
    animate_timeout = gtk_timeout_add( 250,
                                       explore_animate,
                                       NULL);
}

void explore_animate_stop ( void ) {
    g_free(animate_file);
    animate_file = NULL;
    animate_frame = 0;
    if (animate_timeout)
        gtk_timeout_remove(animate_timeout);
    animate_timeout = 0;
}


static gint explore_animate ( gpointer data ) {
    explore_update_file_pm( animate_file,
                            PM_FILE_PENDING + animate_frame);
    animate_frame = (animate_frame + 1) % 4;
    return TRUE;
}


gint iter_sort_strcmp  (GtkTreeModel *model,
                        GtkTreeIter *a,
                        GtkTreeIter *b,
                        gpointer user_data) {
    char * a_str, * b_str;
    int result;
    gtk_tree_model_get (model, a, NAME_COLUMN, &a_str, -1);
    gtk_tree_model_get (model, b, NAME_COLUMN, &b_str, -1);
    result = strcmp(a_str, b_str);
    g_free(a_str);
    g_free(b_str);
    return result;
}

gint compare_str ( gconstpointer a, gconstpointer b) {
    return g_strcasecmp((gchar *) a, (gchar *) b);
}

