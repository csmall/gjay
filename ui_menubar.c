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

#include "gjay.h"
#include "gjay_xmms.h"
#include "ui.h"


static gboolean menuitem_xmms (GtkWidget *widget,
                               GdkEvent *event,
                               gpointer user_data);
static gboolean menuitem_prefs (GtkWidget *widget,
                                GdkEvent *event,
                                gpointer user_data);
static gboolean menuitem_about (GtkWidget *widget,
                                GdkEvent *event,
                                gpointer user_data);


GtkWidget * make_menubar ( void ) {
    GtkWidget * menu_bar, * app_menu;
    GtkWidget * app_item, * xmms_item, 
        * prefs_item, * about_item, * quit_item;
    
    menu_bar = gtk_menu_bar_new();
 
    app_item = gtk_menu_item_new_with_label ("Application");
    gtk_menu_bar_append(GTK_MENU_BAR(menu_bar), app_item);
    
    app_menu = gtk_menu_new ();    

    gtk_menu_item_set_submenu(GTK_MENU_ITEM(app_item), app_menu);

    /* Create the menu items */
    xmms_item = gtk_menu_item_new_with_label ("Select current XMMS song");
    prefs_item = gtk_menu_item_new_with_label ("Preferences...");
    about_item = gtk_menu_item_new_with_label ("About...");
    quit_item  = gtk_menu_item_new_with_label ("Quit");
    
    /* Add them to the menu */
    gtk_menu_shell_append (GTK_MENU_SHELL (app_menu), xmms_item);
    gtk_menu_shell_append (GTK_MENU_SHELL (app_menu), prefs_item);
    gtk_menu_shell_append (GTK_MENU_SHELL (app_menu), about_item);
    gtk_menu_shell_append (GTK_MENU_SHELL (app_menu), quit_item);
    
    /* Attach the callback functions to the activate signal */
    g_signal_connect (G_OBJECT (xmms_item), "activate",
                      G_CALLBACK (menuitem_xmms),
                      NULL);
    g_signal_connect (G_OBJECT (prefs_item), "activate",
                      G_CALLBACK (menuitem_prefs),
                      NULL);
    g_signal_connect (G_OBJECT (about_item), "activate",
                      G_CALLBACK (menuitem_about),
                      NULL);
    g_signal_connect (G_OBJECT (quit_item), "activate",
                      G_CALLBACK (quit_app),
                      NULL);

    gtk_widget_show_all(menu_bar);
    return menu_bar;
}



gboolean menuitem_about (GtkWidget *widget,
                         GdkEvent *event,
                         gpointer user_data) {
    show_about_window();
    return TRUE;
}


gboolean menuitem_prefs (GtkWidget *widget,
                         GdkEvent *event,
                         gpointer user_data) {
    show_prefs_window();
    return TRUE;
}


gboolean menuitem_xmms (GtkWidget *widget,
                        GdkEvent *event,
                        gpointer user_data) {
    song * s;
    gchar * msg; 
    GtkWidget * dialog;

    s = get_current_xmms_song();
    if (s) {
        explore_select_song(s);
    } else {
        if (xmms_is_running()) {
            msg = "Sorry, GJay doesn't appear to know that song";
        } else {
            msg = "Sorry, unable to find XMMS";
        }
        dialog = gtk_message_dialog_new(
            GTK_WINDOW(window),
            GTK_DIALOG_DESTROY_WITH_PARENT,
            GTK_MESSAGE_WARNING,
            GTK_BUTTONS_OK,
            msg);
        g_signal_connect_swapped(GTK_OBJECT(dialog), 
                                 "response",
                                 G_CALLBACK(gtk_widget_destroy),
                                 GTK_OBJECT(dialog));
        gtk_widget_show_all(dialog);
    }
    return TRUE;
}
