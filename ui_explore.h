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

enum explore_show
{
    EXPLORE_NOSEL= 0,
    EXPLORE_DIR,
    EXPLORE_FILE,
    N_EXPLORE_SELECTIONS
};


typedef struct GjayExplore {
    enum explore_show current_selection;
    GtkWidget *widget;
    GtkWidget *selections[N_EXPLORE_SELECTIONS];
    GtkTreeStore *store;
    GtkWidget *tree_view;
    GQueue      *iter_stack;
    GQueue      *parent_name_stack;

    GHashTable  *name_iter_hash;
    GList       *file_name_in_tree;
    GQueue      *files_to_add_queue;
    GList       *files_to_analyze;
    gint        total_files_to_add;
    gint        file_to_add_count;
    gint        animate_timeout;
    gint        animate_frame;
    gchar      *animate_file;
} GjayExplore;


GjayExplore* explore_new(GjayApp *gjay);
void explore_destroy(GjayExplore *explore);
void explore_show(GjayApp *gjay);
void explore_select_song(GjayExplore *explore, GjaySong * s);
void        explore_animate_pending      ( GjayApp *gjay, char * file );
void        explore_animate_stop         ( GjayExplore *explore );
GList * explore_files_in_dir ( const GjayExplore *explore,
                               const gchar * dir,
                               const gboolean recursive);
gboolean explore_path_set_icon (struct GjayExplore *explore,
                                const char *path,
                                GdkPixbuf *pixbuf);
gboolean explore_dir_has_new_songs ( GjayExplore *explore,
                                     GHashTable *name_hash,
                                     const gchar * dir,
                                     const guint verbosity );
gint explore_files_depth_distance ( GjayExplore *explore,
                                    char * file1, 
                                    char * file2 );
void explore_set_root(GjayApp *gjay);
