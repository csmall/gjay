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

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <string.h>
#include "gjay.h"
#include "analysis.h"

#define NUM_APPS 3
static gchar * apps[NUM_APPS] = {
    "mpg321",
    "ogg123",
    "mp3info"
};

static gboolean app_exists (gchar * app);


int main( int argc, char *argv[] ) 
{
    gboolean missing_app = FALSE;
    gint i;

    /* First, check to see if we have all the apps we'll need */
    for (i = 0; i < NUM_APPS; i++) {
        if (!app_exists(apps[i])) {
            fprintf(stderr, "GJay requires %s\n", apps[i]); 
            missing_app = TRUE;
        } 
    } 
    if (!app_exists("xmms")) {
        fprintf(stderr, "GJay strongly suggests xmms\n"); 
    } 
    if(missing_app)
        return 0;
    g_thread_init(NULL);
    srand(time(NULL));
    pthread_mutex_init(&analyze_mutex, NULL);
    pthread_mutex_init(&analyze_data_mutex, NULL);
    gtk_init (&argc, &argv);
    load_prefs();
    load_songs();
    gtk_widget_show_all(make_app_ui());
    gdk_threads_enter();
    gtk_main();
    gdk_threads_leave();
    save_songs();
    save_prefs();
    return(0);
}


static gboolean app_exists (gchar * app) {
    FILE * f;
    char buffer[BUFFER_SIZE];
    gboolean result = FALSE;

    snprintf(buffer, BUFFER_SIZE, "which %s", app); // Yes, I'm lame
    f = popen (buffer, "r");
    while (!feof(f) && fread(buffer, 1, BUFFER_SIZE, f)) 
        result = TRUE;
    pclose(f);
    return result;
}
