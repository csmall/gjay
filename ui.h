/*
 * Gjay - Gtk+ DJ music playlist creator
 * Copyright (C) 2002,2003 Chuck Groom
 * Copyright (C) 2010 Craig Small 
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

#ifndef __UI_H__ 
#define __UI_H__ 


#include "gjay.h"

#define FREQ_IMAGE_WIDTH    NUM_FREQ_SAMPLES
#define FREQ_IMAGE_HEIGHT   15
#define COLOR_IMAGE_WIDTH   FREQ_IMAGE_WIDTH
#define COLOR_IMAGE_HEIGHT  FREQ_IMAGE_HEIGHT

enum {
  TAB_EXPLORE = 0,
  TAB_PLAYLIST,
  TAB_LAST
};

enum
{
    NAME_COLUMN = 0,
    IMAGE_COLUMN,
    N_COLUMNS
};

typedef enum {
    PM_FILE_PENDING = 0,
    PM_FILE_PENDING2,
    PM_FILE_PENDING3,
    PM_FILE_PENDING4,
    PM_FILE_NOSONG,
    PM_FILE_SONG,
    PM_DIR_OPEN,
    PM_DIR_CLOSED,
    PM_DIR_OPEN_NEW,
    PM_DIR_CLOSED_NEW,
    PM_ICON_PENDING,
    PM_ICON_NOSONG,
    PM_ICON_SONG,
    PM_ICON_OPEN,
    PM_ICON_CLOSED,
    PM_ICON_CLOSED_NEW,
    PM_BUTTON_PLAY,
    PM_BUTTON_DIR,
    PM_BUTTON_ALL,
    PM_COLOR_SEL,
    PM_NOT_SET,
    PM_LAST
} pm;


#ifdef WITH_GUI
typedef struct _GjayGUI {
  /* Various Windows */
  GtkWidget *main_window;
  GtkWidget *notebook;
  GtkWidget * prefs_window;
  GtkWidget * message_window;

  GtkWidget *explore_view;
  GtkWidget *explore_hbox;
  GtkWidget *selection_view;
  GtkWidget *playlist_view;
  GtkWidget *playlist_hbox;
  GtkWidget *no_root_view;
  GtkWidget * paned;

  gboolean destroy_window_flag;
  gboolean show_root_dir;

  GdkPixbuf   * pixbufs[50]; //FIXME
  GtkTooltips * tips;
} GjayGUI;

struct play_songs_data {
  struct _GjayPlayer *player;
  GtkWidget *main_window;
  GList *playlist;
};


/* UI utils */
gboolean create_gjay_gui(GjayApp *gjay);
void        set_analysis_progress_visible  ( gboolean visible );
void        set_add_files_progress_visible ( gboolean visible );
void        set_selected_rating_visible   ( gboolean is_visible );
void        set_selected_file             ( GjayApp *gjay, 
											const gchar * file, 
                                            char * short_name, 
                                            const gboolean is_dir );
gboolean daemon_pipe_input (GIOChannel *source,
                            GIOCondition condition,
                            gpointer data);

/* Playlist */
void        set_playlist_rating_visible   ( gboolean is_visible );


gboolean    quit_app                      ( GtkWidget *widget,
                                            GdkEvent *event,
                                            gpointer user_data );
void        gjay_error_dialog(GtkWidget *parent, const gchar *msg);

/* Explore files pane  */
void        explore_view_set_root        ( GjayApp *gjay);
gint        explore_files_depth_distance ( char * file1, 
                                           char * file2 );

/* menubar */
GtkWidget *make_menubar(GjayApp *gjay);
#endif /* WITH_GUI */
#endif /* __UI_H__ */


