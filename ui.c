#include <stdio.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <math.h>
#include <stdlib.h>
#include "gjay.h"
#include "ui.h"
#include "ipc.h"

static char * tabs[TAB_LAST] = {
    "Explore",
    "Make Playlist",
    "Prefs",
    "About"
};

GtkWidget        * window;
GtkWidget        * notebook;
GtkWidget        * explore_view, * selection_view, * playlist_view,
                 * prefs_view, * no_root_view, * about_view;
GtkTooltips      * tips;
static tab_val     current_tab;
static GtkWidget * analysis_label, * analysis_progress;
static GtkWidget * explore_hbox, * playlist_hbox, * prefs_hbox, * about_hbox;
static GtkWidget * paned;
static GtkWidget * msg_window = NULL;
static GtkWidget * msg_text_view = NULL;
static gboolean    destroy_window_flag = FALSE;

GdkPixbuf * pixbufs[PM_LAST];

static char * pixbuf_files[] = {
    "file_pending.png",
    "file_pending2.png",
    "file_pending3.png",
    "file_pending4.png",
    "file_nosong.png",
    "file_song.png",
    "dir_open.png",
    "dir_closed.png",
    "icon_pending.png",
    "icon_nosong.png",
    "icon_song.png",
    "icon_open.png",
    "icon_closed.png",
    "button_play.png",
    "button_dir.png",
    "button_all.png",
    "button_all_recursive.png",
    "star.png",
    "star_small.png",
    "about.png",
    "color_sel.png",
    "not_set.png"
};


static void     load_pixbufs          ( void );
static gboolean delete_window         ( GtkWidget *widget,
                                        GdkEvent *event,
                                        gpointer user_data );
static void     respond_quit_analysis ( GtkDialog *dialog,
                                        gint arg1,
                                        gpointer user_data );
static void     quit_app              ( void);    
static gint     ping_daemon           ( gpointer data );


GtkWidget * make_app_ui ( void ) {
    GtkWidget * hbox1;
    GtkWidget * vbox1;
    GtkWidget * alignment;
    
    tips = gtk_tooltips_new();
    if (prefs.hide_tips) 
        gtk_tooltips_disable(tips);
    else
        gtk_tooltips_enable(tips);

    gdk_rgb_init();
    current_tab = TAB_EXPLORE;
    
    window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title (GTK_WINDOW (window), "GJay");
    gtk_widget_set_size_request (window, APP_WIDTH, APP_HEIGHT);
    gtk_widget_realize(window);
    gtk_signal_connect (GTK_OBJECT (window), "delete_event",
			GTK_SIGNAL_FUNC (delete_window), NULL);
    gtk_container_set_border_width (GTK_CONTAINER (window), 2);
    gtk_widget_realize(window);

    load_pixbufs();

    vbox1 = gtk_vbox_new(FALSE, 2);
    gtk_container_add (GTK_CONTAINER (window), vbox1);
    notebook = gtk_notebook_new(); 
    gtk_box_pack_start(GTK_BOX(vbox1), notebook, TRUE, TRUE, 5);
       
    hbox1 = gtk_hbox_new(FALSE, 2);
    gtk_box_pack_end(GTK_BOX(vbox1), hbox1, FALSE, FALSE, 5);

    analysis_label = gtk_label_new("Idle");
    
    gtk_box_pack_start(GTK_BOX(hbox1), analysis_label, FALSE, FALSE, 5);
    analysis_progress = gtk_progress_bar_new();
    alignment = gtk_alignment_new(0.1, 1, 1, 0);
    gtk_container_add(GTK_CONTAINER(alignment), analysis_progress);
    gtk_box_pack_end(GTK_BOX(hbox1), alignment, FALSE, FALSE, 5);
    gtk_progress_bar_update (GTK_PROGRESS_BAR(analysis_progress),
                             0.0);

    explore_hbox = gtk_hbox_new(FALSE, 2);
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), 
                             explore_hbox,
                             gtk_label_new(tabs[TAB_EXPLORE]));

    playlist_hbox = gtk_hbox_new(FALSE, 2);
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), 
                             playlist_hbox,
                             gtk_label_new(tabs[TAB_PLAYLIST]));

    prefs_hbox = gtk_hbox_new(FALSE, 2);
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), 
                             prefs_hbox,
                             gtk_label_new(tabs[TAB_PREFS]));

    about_hbox = gtk_hbox_new(FALSE, 2);
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), 
                             about_hbox,
                             gtk_label_new(tabs[TAB_ABOUT]));

    explore_view = make_explore_view();
    playlist_view = make_playlist_view();
    selection_view = make_selection_view();
    prefs_view = make_prefs_view();
    no_root_view = make_no_root_view();
    about_view = make_about_view();

    paned = gtk_hpaned_new();
    
    gtk_widget_show_all(explore_view);
    gtk_widget_show_all(playlist_view);
    gtk_widget_show_all(selection_view);
    gtk_widget_show_all(prefs_view);
    gtk_widget_show_all(no_root_view);
    gtk_widget_show_all(about_view);
    
    gtk_signal_connect (GTK_OBJECT (notebook),
                        "switch-page",
                        (GtkSignalFunc) switch_page,
                        NULL);
  
    gtk_notebook_set_page(GTK_NOTEBOOK(notebook), TAB_EXPLORE);
    
    gtk_timeout_add( UI_PING, ping_daemon, NULL);
    return window;
}





gboolean daemon_pipe_input (GIOChannel *source,
                            GIOCondition condition,
                            gpointer data) {
    char buffer[BUFFER_SIZE];
    int len, k, p, l, seek;
    ipc_type ipc, send_ipc;

    read(daemon_pipe_fd, &len, sizeof(int));
    assert(len < BUFFER_SIZE);
    for (k = 0, l = len; l; l -= k) {
        k = read(daemon_pipe_fd, buffer + k, l);
    }

    memcpy((void *) &ipc, buffer, sizeof(ipc_type));
    switch(ipc) {
    case REQ_ACK:
        send_ipc = ACK;
        k = sizeof(ipc_type);
        write(ui_pipe_fd, &k, sizeof(int));
        write(ui_pipe_fd, &send_ipc, sizeof(ipc_type));
        break;
    case ACK:
        // No need for action
        break;
    case STATUS_PERCENT:
        memcpy(&p, buffer + sizeof(ipc_type), sizeof(int));
        if (analysis_progress && !destroy_window_flag) 
            gtk_progress_bar_update (GTK_PROGRESS_BAR(analysis_progress),
                                     p/100.0);
        break;
    case STATUS_TEXT:
        buffer[len] = '\0';
        if (analysis_label && !destroy_window_flag)
            gtk_label_set_text(GTK_LABEL(analysis_label), 
            buffer + sizeof(ipc_type));
        break;
    case ADDED_FILE:
        memcpy(&seek, buffer + sizeof(ipc_type), sizeof(int));
        add_from_daemon_file_at_seek(seek);
        break;
    case ANIMATE_START:
        buffer[len] = '\0';
        if (!destroy_window_flag)
            explore_animate_pending(buffer + sizeof(ipc_type));
        break;
    case ANIMATE_STOP:
        explore_animate_stop();
        break;
    default:
        // Do nothing
    }    
    return TRUE;
}


void display_message ( gchar * msg ) {
    GtkWidget * button, * swin, * vbox = NULL;
    GtkTextBuffer *buffer;
    
    if (msg_window == NULL) {
        msg_window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
        gtk_window_set_title (GTK_WINDOW (msg_window), "GJay: Messages");
        gtk_widget_set_usize(msg_window, MSG_WIDTH, MSG_HEIGHT);
        gtk_container_set_border_width (GTK_CONTAINER (msg_window), 5);

        vbox = gtk_vbox_new (FALSE, 2);
        gtk_container_add (GTK_CONTAINER (msg_window), vbox);

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

        button = gtk_button_new_with_label("OK");
        gtk_box_pack_start(GTK_BOX(vbox), button, FALSE, FALSE, 5);

        g_signal_connect_swapped (G_OBJECT (msg_window), 
                                  "delete_event",
                                  G_CALLBACK(gtk_widget_hide),
                                  G_OBJECT (msg_window));
        g_signal_connect_swapped (G_OBJECT (button), 
                                  "clicked",
                                  G_CALLBACK (gtk_widget_hide),
                                  G_OBJECT (msg_window));
    }
    
    buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (msg_text_view));
    gtk_text_buffer_insert_at_cursor(buffer, 
                                     msg, 
                                     strlen(msg));
    gtk_text_buffer_insert_at_cursor(buffer, "\n", 1);
    gtk_widget_show_all (GTK_WIDGET(msg_window));
}


void switch_page (GtkNotebook *notebook,
                  GtkNotebookPage *page,
                  gint page_num,
                  gpointer user_data) {
    if (destroy_window_flag)
        return;
    
    if (explore_view->parent) {
        g_object_ref(G_OBJECT(explore_view));
        gtk_container_remove(GTK_CONTAINER(explore_view->parent), 
                             explore_view);
    }
    if (selection_view->parent) {
        gtk_object_ref(GTK_OBJECT(selection_view));
        gtk_container_remove(GTK_CONTAINER(selection_view->parent),
                             selection_view);
    }
    if (about_view->parent) {
        gtk_object_ref(GTK_OBJECT(about_view));
        gtk_container_remove(GTK_CONTAINER(about_view->parent),
                             about_view);
    }
    if (playlist_view->parent) {
        gtk_object_ref(GTK_OBJECT(playlist_view));
        gtk_container_remove(GTK_CONTAINER(playlist_view->parent),
                             playlist_view);
    }
    if (prefs_view->parent) {
        gtk_object_ref(GTK_OBJECT(prefs_view));
        gtk_container_remove(GTK_CONTAINER(prefs_view->parent),
                             prefs_view);
    }
    if (no_root_view->parent) {
        gtk_object_ref(GTK_OBJECT(no_root_view));
        gtk_container_remove(GTK_CONTAINER(no_root_view->parent),
                             no_root_view);
    }
    if (paned->parent) {
        gtk_object_ref(GTK_OBJECT(paned));
        gtk_container_remove(GTK_CONTAINER(paned->parent),
                             paned);
    }
    switch (page_num) {
    case TAB_EXPLORE:
        if (prefs.song_root_dir) {
            gtk_box_pack_start(GTK_BOX(explore_hbox), paned,
                               TRUE, TRUE, 5);
            gtk_paned_add1(GTK_PANED(paned), explore_view); 
            gtk_paned_add2(GTK_PANED(paned), selection_view); 
            set_selected_in_playlist_view(FALSE);
            gtk_widget_show(paned);
        } else {
             gtk_box_pack_start(GTK_BOX(explore_hbox), no_root_view,
                               TRUE, TRUE, 5);
        }
        gtk_widget_show(explore_hbox);
        break;
    case TAB_PLAYLIST:
        gtk_box_pack_start(GTK_BOX(playlist_hbox), paned, TRUE, TRUE, 5);
        gtk_paned_add1(GTK_PANED(paned), playlist_view);
        gtk_paned_add2(GTK_PANED(paned), selection_view);
        set_selected_in_playlist_view(TRUE);
        gtk_widget_show(paned);
        gtk_widget_show(playlist_hbox);
        break;
    case TAB_PREFS:
        gtk_box_pack_start(GTK_BOX(prefs_hbox), prefs_view, TRUE, TRUE, 5);
        gtk_widget_show(prefs_hbox);
        break;
    case TAB_ABOUT:
        gtk_box_pack_start(GTK_BOX(about_hbox), about_view, TRUE, TRUE, 0);
        gtk_widget_show(about_hbox);
        break;
    }
}



/* Load a pixbuf from...
 *    ./icons/
 *    ~/.gjay/icons
 *    /usr/share/gjay/icons
 *    /usr/local/share/gjay/icons
 */
void load_pixbufs(void) {
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
                                      int type) {
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


gboolean delete_window (GtkWidget *widget,
                         GdkEvent *event,
                         gpointer user_data) {
    GtkWidget * dialog;

    if (prefs.daemon_action == PREF_DAEMON_ASK) {
        dialog = gtk_message_dialog_new(GTK_WINDOW(widget),
                                        GTK_DIALOG_DESTROY_WITH_PARENT,
                                        GTK_MESSAGE_QUESTION,
                                        GTK_BUTTONS_YES_NO,
                                        "Continue analysis in background?");
        g_signal_connect (GTK_OBJECT (dialog), 
                          "response",
                          G_CALLBACK (respond_quit_analysis),
                          NULL);
        gtk_widget_show(dialog);
        prefs.detach = FALSE;
        return TRUE;
    } else {
        quit_app();
    }
    return FALSE;
}


static void respond_quit_analysis (GtkDialog *dialog,
                                   gint arg1,
                                   gpointer user_data) {
    if (arg1 == GTK_RESPONSE_YES) {
        prefs.detach = TRUE;
    } else {
        prefs.detach = FALSE;
    }
    quit_app();
}



static void quit_app (void) {
    
    destroy_window_flag = TRUE;
    gtk_object_sink(GTK_OBJECT(tips));
    gtk_widget_destroy(explore_view);
    gtk_widget_destroy(playlist_view);
    gtk_widget_destroy(selection_view);
    gtk_widget_destroy(prefs_view);
    gtk_widget_destroy(no_root_view);
    gtk_widget_destroy(about_view);
    gtk_widget_destroy(paned);
    gtk_main_quit();
}


/**
 * We make sure to ping the daemon periodically such that it knows the
 * UI process is still attached. Otherwise, it will timeout after
 * about 20 seconds and quit.
 */
static gint ping_daemon ( gpointer data ) {
    send_ipc(ui_pipe_fd, ACK);
    return TRUE;
}
