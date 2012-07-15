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

#define APP_WIDTH  620
#define APP_HEIGHT 470
#define PLAYLIST_WIDTH  500
#define PLAYLIST_HEIGHT 250
#define MSG_WIDTH  250
#define MSG_HEIGHT 160
#define FREQ_IMAGE_WIDTH    NUM_FREQ_SAMPLES
#define FREQ_IMAGE_HEIGHT   15
#define COLOR_IMAGE_WIDTH   FREQ_IMAGE_WIDTH
#define COLOR_IMAGE_HEIGHT  FREQ_IMAGE_HEIGHT
#define COLORWHEEL_DIAMETER 150
#define COLORWHEEL_SELECT   3
#define COLORWHEEL_SPACING  5
#define COLORWHEEL_V_SWATCH_WIDTH 0.2
#define COLORWHEEL_V_HEIGHT       0.7
#define COLORWHEEL_SWATCH_HEIGHT  0.2


typedef enum {
    TAB_EXPLORE = 0,
    TAB_PLAYLIST,
    TAB_LAST
} tab_val;

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

#define PM_ABOUT    "about.png"


#ifdef WITH_GUI
/* UI utils */
GdkPixbuf * load_gjay_pixbuf(const char *filename);
GtkWidget * new_button_label_pixbuf      ( char * label_text, 
                                           int pm_type);
void        switch_page (GtkNotebook *notebook,
                         GtkNotebookPage *page,
                         gint page_num,
                         gpointer user_data);
void        make_app_ui     ( void );


GtkWidget * make_explore_view ( void );
GtkWidget * make_playlist_view ( void );
GtkWidget * make_no_root_view ( void );
GtkWidget * make_selection_view ( void );
GtkWidget * make_prefs_window ( void );
GtkWidget * make_about_view ( void );
GtkWidget * make_message_window( void);
void        show_about_window ( void );
void        hide_about_window ( void );
void        show_prefs_window ( void );
void        hide_prefs_window ( void );
void        prefs_update_song_dir ( void );
void        set_analysis_progress_visible  ( gboolean visible );
void        set_add_files_progress_visible ( gboolean visible );
void        set_add_files_progress         ( char * str,
                                             gint percent );

gboolean    quit_app                      ( GtkWidget *widget,
                                            GdkEvent *event,
                                            gpointer user_data );
void        gjay_error_dialog(char *msg);

/* Explore files pane  */
void        explore_view_set_root        ( const gchar * root_dir );
gint        explore_view_set_root_idle   ( gpointer data );
gboolean    explore_update_path_pm       ( char * path,
                                           int type );
GList *     explore_files_in_dir         ( char * dir, 
                                           gboolean recursive );
GList *     explore_dirs_in_dir          ( char * dir );
gint        explore_files_depth_distance ( char * file1, 
                                           char * file2 );
void        explore_animate_pending      ( char * file );
void        explore_animate_stop         ( void );
gboolean    explore_dir_has_new_songs    ( char * dir );
void        explore_select_song          ( song * s);

/* Select file pane */
void        set_selected_file             ( char * file, 
                                            char * short_name, 
                                            gboolean is_dir );
void        update_selection_area         ( void );
void        set_selected_in_playlist_view ( gboolean in_view );
void        set_selected_rating_visible   ( gboolean is_visible );

/* Colorwheel widget (in select file pane) */
GtkWidget * create_colorwheel             ( gint diameter,
                                            GList ** list,
                                            GFunc change_func,
                                            gpointer user_data);
HSV         get_colorwheel_color          ( GtkWidget * colorwheel );
void        set_colorwheel_color          ( GtkWidget * colorwheel,
                                            HSV color );
/* Playlist */
void        set_playlist_rating_visible   ( gboolean is_visible );

/* Menu */
GtkWidget * make_menubar                ( void );

#else
#define gjay_error_dialog(msg) g_error(msg)
#endif /* WITH_GUI */
#endif /* __UI_H__ */


