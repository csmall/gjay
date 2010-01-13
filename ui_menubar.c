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
#include "gjay_audacious.h"
#include "ui.h"


static void menuitem_currentsong (void);
static void menuitem_quit (void);

static GtkItemFactoryEntry menu_items[] = {
    //{ "/_Application", NULL, NULL, 0, "<Branch>" },
    { "/_File/_Go to current song", "<control>G", menuitem_currentsong, 
      0, "<Item>" },
    { "/_Edit/_Preferences...", NULL, show_prefs_window, 0, "<Item>" },
    { "/_File/_Quit", "<control>Q", menuitem_quit, 0, "<Item>" },
    { "/_Help/_About...", NULL, show_about_window, 0, "<Item>" }
};
static gint nmenu_items = sizeof(menu_items) / sizeof(GtkItemFactoryEntry);

GtkWidget * make_menubar ( void ) {
    GtkItemFactory * factory;
    GtkAccelGroup * accel_group;
    
    accel_group = gtk_accel_group_new();
    factory = gtk_item_factory_new(GTK_TYPE_MENU_BAR, "<main>", accel_group);
    gtk_item_factory_create_items(factory, nmenu_items, menu_items, NULL);
    gtk_window_add_accel_group(GTK_WINDOW(gjay->main_window), accel_group);
    
    return gtk_item_factory_get_widget(factory, "<main>");
}


void menuitem_currentsong (void) {
    song * s;
    gchar * msg; 
    GtkWidget * dialog;

    s = get_current_audacious_song();
    if (s) {
        explore_select_song(s);
    } else {
        if (audacious_is_running()) {
            msg = "Sorry, GJay doesn't appear to know that song";
        } else {
            msg = "Sorry, unable to connect to Audacious.\nIs Audacious running?";
        }
        dialog = gtk_message_dialog_new(
            GTK_WINDOW(gjay->window),
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
}

static void menuitem_quit (void) {
    quit_app(NULL, NULL, NULL);
}
