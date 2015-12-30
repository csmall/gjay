/*
 * Gjay - Gtk+ DJ music playlist creator
 * Copyright (C) 2002,2003 Chuck Groom
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

/* Only thr ui*.c files should include this */
#ifndef __UI_PRIVATE_H__ 
#define __UI_PRIVATE_H__

#define APP_WIDTH  620
#define APP_HEIGHT 470
#define PLAYLIST_WIDTH  500
#define PLAYLIST_HEIGHT 250
#define MSG_WIDTH  250
#define MSG_HEIGHT 160
#define COLORWHEEL_DIAMETER 150

#define PM_ABOUT    "about.png"

#ifdef WITH_GUI


GtkWidget * new_button_label_pixbuf      ( char * label_text,
                                           int pm_type,
                                           GdkPixbuf **pixbufs);

void        switch_page (GtkNotebook *notebook,
                         GtkWidget *page,
                         gint page_num,
                         gpointer user_data);
GtkWidget * make_playlist_view ( GjayApp *gjay );
GtkWidget * make_no_root_view ( GjayApp *gjay );
GtkWidget * make_selection_view ( GjayApp *gjay );
GtkWidget * make_prefs_window ( GjayApp *gjay );
GtkWidget * make_about_view ( void );
void        show_about_window ( GtkAction *action, gpointer user_data );
void        hide_about_window ( void );
void        set_add_files_progress         ( char * str,
                                             gint percent );

/* Select file pane */
void        set_selected_in_playlist_view ( GjayApp *gjay,
                                            const gboolean in_view );

/* Colorwheel widget (in select file pane) */
GtkWidget * create_colorwheel             ( GjayApp *gjay,
                                            const gint diameter,
                                            GList ** list,
                                            GFunc change_func,
                                            gpointer user_data);
HSV         get_colorwheel_color          ( GtkWidget * colorwheel );
void        set_colorwheel_color          ( GtkWidget * colorwheel,
                                            HSV color );

/* Menu */
GtkWidget * make_menubar                ( GjayApp *gjay );

#endif /* WITH_GUI */

#endif /* _UI_PRIVATE_H_ */
