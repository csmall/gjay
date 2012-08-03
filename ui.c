/*
 * Gjay - Gtk+ DJ music playlist creator
 * Copyright (C) 2002 Chuck Groom
 * Copyright (C) 2010-2012 Craig Small 
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
#include <stdio.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <math.h>
#include <stdlib.h>
#include "gjay.h"
#include "ui.h"
#include "ui_private.h"
#include "ipc.h"
#include "i18n.h"

static char * tabs[TAB_LAST] = {
    "Explore",
    "Make Playlist"
};

static GtkWidget * analysis_label, * analysis_progress;
static GtkWidget * add_files_label, * add_files_progress;
static GdkPixbuf * load_gjay_pixbuf(const char *filename);
static GtkWidget * plugin_pane[1];

static char * pixbuf_files[] = {
    "file_pending.png",
    "file_pending2.png",
    "file_pending3.png",
    "file_pending4.png",
    "file_nosong.png",
    "file_song.png",
    "dir_open.png",
    "dir_closed.png",
    "dir_open_new.png",
    "dir_closed_new.png",
    "icon_pending.png",
    "icon_nosong.png",
    "icon_song.png",
    "icon_open.png",
    "icon_closed.png",
    "icon_closed_new.png",
    "button_play.png",
    "button_dir.png",
    "button_all.png",
    "color_sel.png",
    "not_set.png"
};


static void     load_pixbufs          ( GdkPixbuf **pixbufs );
static void     respond_quit_analysis ( GtkDialog *dialog,
                                        gint arg1,
                                        gpointer user_data );
static void     destroy_app           ( GjayGUI *ui);    
static GtkWidget * make_message_window( void);
void gjay_message_log(const gchar *log_domain, 
    GLogLevelFlags log_level,
    const gchar *message,
    gpointer user_data);

gboolean create_gjay_gui ( GjayApp *gjay) {
    GjayGUI *ui;

    GtkWidget * vbox1, * hbox1, * hbox2;
    GtkWidget * alignment, * menubar;
    

	if ((ui = g_malloc0(sizeof(GjayGUI))) == NULL) {
	  g_warning( _("Unable to allocate memory for GUI.\n"));
	  return FALSE;
	}
	gjay->gui = ui;
    ui->destroy_window_flag = FALSE;
	ui->show_root_dir = FALSE;
	if (gjay->prefs && gjay->prefs->song_root_dir)
	  ui->show_root_dir = TRUE;

    /* FIXME
    if (prefs.hide_tips) 
        gtk_tooltips_disable(tips);
    else
        gtk_tooltips_enable(tips);
        */

    gdk_rgb_init();
    
    ui->main_window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title (GTK_WINDOW (ui->main_window), "GJay");
    gtk_widget_set_size_request (ui->main_window, APP_WIDTH, APP_HEIGHT);
    gtk_widget_realize(ui->main_window);
    gtk_container_set_border_width (GTK_CONTAINER (ui->main_window), 2);
    gtk_widget_realize(ui->main_window);

    load_pixbufs(ui->pixbufs);

    vbox1 = gtk_vbox_new(FALSE, 2);
    gtk_container_add (GTK_CONTAINER (ui->main_window), vbox1);

    menubar = make_menubar(gjay);
    gtk_box_pack_start(GTK_BOX(vbox1), menubar, FALSE, FALSE, 1);

    ui->notebook = gtk_notebook_new(); 
    gtk_box_pack_start(GTK_BOX(vbox1), ui->notebook, TRUE, TRUE, 3);
       
    hbox1 = gtk_hbox_new(FALSE, 2);
    gtk_box_pack_end(GTK_BOX(vbox1), hbox1, FALSE, FALSE, 2);

    analysis_label = gtk_label_new("Idle");    
    gtk_box_pack_start(GTK_BOX(hbox1), analysis_label, FALSE, FALSE, 5);
    add_files_label = gtk_label_new("");
    gtk_box_pack_start(GTK_BOX(hbox1), add_files_label, FALSE, FALSE, 5);

    alignment = gtk_alignment_new(0.1, 1, 1, 0);
    gtk_box_pack_end(GTK_BOX(hbox1), alignment, FALSE, FALSE, 5);
    hbox2 = gtk_hbox_new(FALSE, 0);
    gtk_container_add(GTK_CONTAINER(alignment), hbox2);

    analysis_progress = gtk_progress_bar_new();
    gtk_progress_bar_update (GTK_PROGRESS_BAR(analysis_progress),
                             0.0);
    gtk_box_pack_start(GTK_BOX(hbox2), analysis_progress, FALSE, FALSE, 0);

    add_files_progress = gtk_progress_bar_new();
    gtk_progress_bar_update (GTK_PROGRESS_BAR(add_files_progress),
                             0.0);
    gtk_box_pack_start(GTK_BOX(hbox2), add_files_progress, FALSE, FALSE, 0);

    ui->explore_hbox = gtk_hbox_new(FALSE, 2);
    gtk_notebook_append_page(GTK_NOTEBOOK(ui->notebook), 
                             ui->explore_hbox,
                             gtk_label_new(tabs[TAB_EXPLORE]));

    ui->playlist_hbox = gtk_hbox_new(FALSE, 2);
    gtk_notebook_append_page(GTK_NOTEBOOK(ui->notebook), 
                             ui->playlist_hbox,
                             gtk_label_new(tabs[TAB_PLAYLIST]));

    ui->message_window = make_message_window();
    ui->explore_view = make_explore_view(gjay);
    ui->playlist_view = make_playlist_view(gjay);
    ui->selection_view = make_selection_view(gjay);
    ui->no_root_view = make_no_root_view(gjay);

    ui->paned = gtk_hpaned_new();
    
    gtk_widget_show_all(ui->explore_view);
    gtk_widget_show_all(ui->playlist_view);
    gtk_widget_show_all(ui->selection_view);
    gtk_widget_show_all(ui->no_root_view);
    
    gtk_signal_connect (GTK_OBJECT (ui->notebook),
                        "switch-page",
                        (GtkSignalFunc) switch_page,
                        gjay);
  
  gtk_notebook_set_page(GTK_NOTEBOOK(ui->notebook), TAB_EXPLORE);
    

  ui->prefs_window = make_prefs_window(gjay);

  g_log_set_handler(NULL,
        G_LOG_LEVEL_WARNING | G_LOG_FLAG_FATAL | G_LOG_FLAG_RECURSION,
        &gjay_message_log, ui->message_window);

  gtk_widget_show_all(ui->main_window);
  return TRUE;
}





gboolean daemon_pipe_input (GIOChannel *source,
                            GIOCondition condition,
                            gpointer user_data) {
    char buffer[BUFFER_SIZE];
    gchar * str;
    int len, k, p, l, seek;
    ipc_type ipc, send_ipc;
    GList * ll;
    gboolean update;
    GjaySong * s;

	GjayApp *gjay = (GjayApp*)user_data;

    read(gjay->ipc->daemon_fifo, &len, sizeof(int));
    assert(len < BUFFER_SIZE);
    for (k = 0, l = len; l; l -= k) {
        k = read(gjay->ipc->daemon_fifo, buffer + k, l);
    }

    memcpy((void *) &ipc, buffer, sizeof(ipc_type));
    switch(ipc) {
    case REQ_ACK:
        send_ipc = ACK;
        k = sizeof(ipc_type);
        write(gjay->ipc->ui_fifo, &k, sizeof(int));
        write(gjay->ipc->ui_fifo, &send_ipc, sizeof(ipc_type));
        break;
    case ACK:
        // No need for action
        break;
    case STATUS_PERCENT:
        memcpy(&p, buffer + sizeof(ipc_type), sizeof(int));
        if (analysis_progress && !gjay->gui->destroy_window_flag) 
            gtk_progress_bar_update (GTK_PROGRESS_BAR(analysis_progress),
                                     p/100.0);
        break;
    case STATUS_TEXT:
        buffer[len] = '\0';
        if (analysis_label && !gjay->gui->destroy_window_flag) {
            str = buffer + sizeof(ipc_type);
            gtk_label_set_text(GTK_LABEL(analysis_label), str);
        }
        break;
    case ADDED_FILE:
        /* Unmark all songs */
        for (ll = g_list_first(gjay->songs->songs); ll; ll = g_list_next(ll)) 
            SONG(ll)->marked = FALSE;
        memcpy(&seek, buffer + sizeof(ipc_type), sizeof(int));
        if (add_from_daemon_file_at_seek(gjay, seek) == TRUE)
		  gjay->songs->dirty = TRUE;
        update = FALSE;
        /* Update visible marked songs */
        for (ll = g_list_first(gjay->songs->songs); ll; ll = g_list_next(ll)) { 
            s = SONG(ll);
            if (s->marked && !s->no_data) {
                /* Change the tree view icon and selection view, if 
                 * necessary. Note that song paths are latin-1 */
                explore_update_path_pm(gjay->gui->pixbufs, s->path, PM_FILE_SONG);
                if (!update && g_list_find(gjay->selected_songs, s)) {
                    update_selection_area(gjay->selected_songs);
                    update = TRUE;
                }
            }
            SONG(ll)->marked = FALSE;
        }
        break;
    case ANIMATE_START:
        /* Animate the filename with the given path (latin-1) */
        buffer[len] = '\0';
        if (!gjay->gui->destroy_window_flag)
            explore_animate_pending(gjay->gui, buffer + sizeof(ipc_type));
        break;
    case ANIMATE_STOP:
        explore_animate_stop();
        break;
    default:
        // Do nothing
        break;
    }    
    return TRUE;
}

static GtkWidget * make_message_window( void)
{
  GtkWidget *window , *vbox, *swin, *msg_text_view, *button;

  window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  gtk_window_set_title (GTK_WINDOW (window), _("GJay: Messages"));
  gtk_widget_set_usize(window, MSG_WIDTH, MSG_HEIGHT);
  gtk_container_set_border_width (GTK_CONTAINER (window), 5);

  vbox = gtk_vbox_new (FALSE, 2);
  gtk_container_add (GTK_CONTAINER (window), vbox);

  swin = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (swin),
      GTK_POLICY_NEVER,
      GTK_POLICY_AUTOMATIC);
  gtk_box_pack_start(GTK_BOX(vbox), swin, TRUE, TRUE, 2);
  msg_text_view = gtk_text_view_new ();
  gtk_text_view_set_editable(GTK_TEXT_VIEW(msg_text_view), FALSE);
  gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(msg_text_view),
      GTK_WRAP_WORD);
  gtk_container_add(GTK_CONTAINER(swin), msg_text_view);
  g_object_set_data(G_OBJECT(window), "text_view", msg_text_view);

  button = gtk_button_new_from_stock(GTK_STOCK_OK);
  gtk_box_pack_start(GTK_BOX(vbox), button, FALSE, FALSE, 5);

  g_signal_connect_swapped (G_OBJECT (window), 
      "delete_event",
      G_CALLBACK(gtk_widget_hide),
      G_OBJECT (window));
  g_signal_connect_swapped (G_OBJECT (button), 
      "clicked",
      G_CALLBACK (gtk_widget_hide),
      G_OBJECT (window));
  GTK_WIDGET_SET_FLAGS (button, GTK_CAN_DEFAULT);
  gtk_widget_grab_default (button);
  gtk_widget_grab_focus (button);

  return window;
}


void switch_page (GtkNotebook *notebook,
                  GtkNotebookPage *page,
                  gint page_num,
                  gpointer user_data) {

  GjayApp *gjay = (GjayApp*)user_data;
  GjayGUI *ui = gjay->gui;
    if (ui->destroy_window_flag)
        return;
    
    if (ui->explore_view->parent) {
        g_object_ref(G_OBJECT(ui->explore_view));
        gtk_container_remove(GTK_CONTAINER(ui->explore_view->parent), 
                             ui->explore_view);
    }
    if (ui->selection_view->parent) {
        gtk_object_ref(GTK_OBJECT(ui->selection_view));
        gtk_container_remove(GTK_CONTAINER(ui->selection_view->parent),
                             ui->selection_view);
    }
    if (ui->playlist_view->parent) {
        gtk_object_ref(GTK_OBJECT(ui->playlist_view));
        gtk_container_remove(GTK_CONTAINER(ui->playlist_view->parent),
                             ui->playlist_view);
    }
    if (ui->no_root_view->parent) {
        gtk_object_ref(GTK_OBJECT(ui->no_root_view));
        gtk_container_remove(GTK_CONTAINER(ui->no_root_view->parent),
                             ui->no_root_view);
    }
    if (ui->paned->parent) {
        gtk_object_ref(GTK_OBJECT(ui->paned));
        gtk_container_remove(GTK_CONTAINER(ui->paned->parent),
                             ui->paned);
    }
    switch (page_num) {
    case TAB_EXPLORE:
        if (ui->show_root_dir) {
            gtk_box_pack_start(GTK_BOX(ui->explore_hbox), ui->paned,
                               TRUE, TRUE, 5);
            gtk_paned_add1(GTK_PANED(ui->paned), ui->explore_view); 
            gtk_paned_add2(GTK_PANED(ui->paned), ui->selection_view); 
            set_selected_in_playlist_view(gjay, FALSE);
            gtk_widget_show(ui->paned);
        } else {
             gtk_box_pack_start(GTK_BOX(ui->explore_hbox), ui->no_root_view,
                               TRUE, TRUE, 5);
        }
        gtk_widget_show(ui->explore_hbox);
        break;
    case TAB_PLAYLIST:
        gtk_box_pack_start(GTK_BOX(ui->playlist_hbox), ui->paned, TRUE, TRUE, 5);
        gtk_paned_add1(GTK_PANED(ui->paned), ui->playlist_view);
        gtk_paned_add2(GTK_PANED(ui->paned), ui->selection_view);
        set_selected_in_playlist_view(gjay, TRUE);
        gtk_widget_show(ui->paned);
        gtk_widget_show(ui->playlist_hbox);
        break;
    default:
        /* Plug-ins */
        gtk_widget_show(plugin_pane[page_num - TAB_LAST]);
        break;
    }
}

static GdkPixbuf *
load_gjay_pixbuf(const char *filename)
{
  GdkPixbuf *pb = NULL;
  gchar *path;
  GError *error=NULL;
  int i=0;

  while(1)
  {
    switch(i++)
    {
      case 0:
        path = g_strdup_printf("icons/%s", filename);
        break;
      case 1:
        path = g_strdup_printf("%s/%s/%s", getenv("HOME"), GJAY_DIR, filename);
        break;
      case 2:
        path = g_strdup_printf("/usr/share/gjay/icons/%s", filename);
        break;
      case 3:
        path = g_strdup_printf("/usr/local/share/gjay/icons/%s", filename);
        break;
      default:
        return NULL;
    }
    pb =  gdk_pixbuf_new_from_file(path, &error);
    if (pb != NULL)
      return pb;
    error = NULL;
    g_free(path);
  } /* while */
  /* should never get here */
  return NULL;
}
      




/* Load a pixbuf from...
 *    ./icons/
 *    ~/.gjay/icons
 *    /usr/share/gjay/icons
 *    /usr/local/share/gjay/icons
 */
void load_pixbufs(GdkPixbuf **pixbufs) {
    char buffer[BUFFER_SIZE];
    char * file;
    GdkPixbuf * pb;
    GError *error = NULL;
    int type;

    for(type = 0; type < PM_LAST; type++) {
        file = pixbuf_files[type];
        snprintf(buffer, BUFFER_SIZE, "%s/%s", "icons", file);
        pb =  gdk_pixbuf_new_from_file(buffer, &error);
        if (!pb) {
            error = NULL;
            snprintf(buffer, BUFFER_SIZE, "%s/%s/icons/%s",  
                     getenv("HOME"), GJAY_DIR, file);
            pb =  gdk_pixbuf_new_from_file(buffer, &error);
        } 
        if(!pb) {
            error = NULL;
            snprintf(buffer, BUFFER_SIZE, "/usr/share/gjay/icons/%s",  
                     file);
            pb =  gdk_pixbuf_new_from_file(buffer, &error);
        }
        if(!pb) {
            error = NULL;
            snprintf(buffer, BUFFER_SIZE, "/usr/local/share/gjay/icons/%s",  
                     file);
            pb =  gdk_pixbuf_new_from_file(buffer, &error);
        }
        pixbufs[type] = pb;
    }
} 



GtkWidget * new_button_label_pixbuf ( char * label_text, 
                                      int type,
									  GdkPixbuf **pixbufs) {
    GtkWidget * button, * hbox, * label, * image;

    button = gtk_button_new();
    
    hbox = gtk_hbox_new (FALSE, 2);
    gtk_container_add(GTK_CONTAINER(button), hbox);
    
    image = gtk_image_new_from_pixbuf(pixbufs[type]);
    label = gtk_label_new (label_text);
    gtk_box_pack_start(GTK_BOX(hbox), image, TRUE, TRUE, 2);
    gtk_box_pack_start(GTK_BOX(hbox), label, TRUE, TRUE, 2);    

    return button;
}


gboolean quit_app (GtkWidget *widget,
                   GdkEvent *event,
                   gpointer user_data) {
    GtkWidget * dialog;
    
	GjayApp *gjay = (GjayApp*)user_data;

    if (gjay->prefs->daemon_action == PREF_DAEMON_ASK) {
        dialog = gtk_message_dialog_new(GTK_WINDOW(widget),
                                        GTK_DIALOG_DESTROY_WITH_PARENT,
                                        GTK_MESSAGE_QUESTION,
                                        GTK_BUTTONS_YES_NO,
                                        _("Continue analysis in background?"));
        g_signal_connect (GTK_OBJECT (dialog), 
                          "response",
                          G_CALLBACK (respond_quit_analysis),
                          gjay);
        gtk_widget_show(dialog);
        gjay->prefs->detach = FALSE;
        return TRUE;
    } else {
        destroy_app(gjay->gui);
    }
    return FALSE;
}


static void respond_quit_analysis (GtkDialog *dialog,
                                   gint arg1,
                                   gpointer user_data) {

    GjayApp *gjay = (GjayApp*)user_data;

    if (arg1 == GTK_RESPONSE_YES) {
        gjay->prefs->detach = TRUE;
    } else {
        gjay->prefs->detach = FALSE;
    }
    destroy_app(gjay->gui);
}



static void destroy_app ( GjayGUI *ui ) {    
    ui->destroy_window_flag = TRUE;
    gtk_widget_destroy(ui->explore_view);
    gtk_widget_destroy(ui->playlist_view);
    gtk_widget_destroy(ui->selection_view);
    gtk_widget_destroy(ui->prefs_window);
    gtk_widget_destroy(ui->no_root_view);
    gtk_widget_destroy(ui->paned);
    gtk_main_quit();
}


void set_analysis_progress_visible  ( gboolean visible ) {
    if (visible) {
        gtk_widget_show(analysis_label);
        gtk_widget_show(analysis_progress);
    } else {
        gtk_widget_hide(analysis_label);
        gtk_widget_hide(analysis_progress);
    }
}


void set_add_files_progress_visible ( gboolean visible ) {
    if (visible) {
        gtk_widget_show(add_files_label);
        gtk_widget_show(add_files_progress);
    } else {
        gtk_widget_hide(add_files_label);
        gtk_widget_hide(add_files_progress);
    }
}


#define ADD_PATH_CUTOFF 50
void set_add_files_progress ( char * str,
                              gint percent ) {
    char buffer[BUFFER_SIZE];
    int len;
    gboolean add_elipsis = FALSE;

    if (str) {
        len = strlen(str);
        if (len > ADD_PATH_CUTOFF) {
            str = str + len - ADD_PATH_CUTOFF;
            add_elipsis = TRUE;
        }
        snprintf(buffer, BUFFER_SIZE, "Add: %s%s",
                 add_elipsis ? "..." : "",
                 str);
        gtk_label_set_text(GTK_LABEL(add_files_label), buffer);
    }
    gtk_progress_bar_update (GTK_PROGRESS_BAR(add_files_progress),
                             percent/100.0);
}

void show_about_window( GtkAction *action, gpointer user_data ) {

  static const gchar * const authors[] = {
    "Chuck Groom",
    "Craig Small <csmall@enc.com.au>",
    NULL
  };
  static const gchar copyright[] = \
    "Copyright \xc2\xa9 2004 Chuck Groom\n"
    "Copyright \xc2\xa9 2010-2011 Craig Small";

  static const gchar comments[] = \
    "GTK+ playlist generator for a collection of music based upon "
    "automatically analyzed song characteristics as well as "
    "user-assigned categorizations.";

  gtk_show_about_dialog(NULL,
      "authors", authors,
      "comments", comments,
      "copyright", copyright,
      "logo", load_gjay_pixbuf(PM_ABOUT),
      "version", VERSION,
      "website", "http://gjay.sourceforge.net/",
      "program-name", "GJay",
      NULL);
}


void gjay_error_dialog(GtkWidget *parent, const gchar *msg) {
  GtkWidget *dialog;

  dialog = gtk_message_dialog_new(GTK_WINDOW(parent),
      GTK_DIALOG_DESTROY_WITH_PARENT,
      GTK_MESSAGE_ERROR,
      GTK_BUTTONS_CLOSE,
      "%s", msg);
  gtk_dialog_run(GTK_DIALOG(dialog));
  gtk_widget_destroy(dialog);
}

void gjay_message_log(const gchar *log_domain, 
    GLogLevelFlags log_level,
    const gchar *message,
    gpointer user_data)
{
  GtkWidget *msg_window = GTK_WIDGET(user_data);
  GtkWidget *msg_text_view;
  GtkTextBuffer *buffer;

  msg_text_view = GTK_WIDGET(g_object_get_data(G_OBJECT(msg_window), "text_view"));

  buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (msg_text_view));
  gtk_text_buffer_insert_at_cursor(buffer, 
      message, 
      strlen(message));
  gtk_text_buffer_insert_at_cursor(buffer, "\n", 1);
  gtk_widget_show_all (GTK_WIDGET(msg_window));
}
