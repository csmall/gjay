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

#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <xmms/xmmsctrl.h>
#include "gjay.h" 
#include "gjay_xmms.h"
#include "songs.h" 

static gint xmms_session = -1;

#define MAX_XMMS_SESSION 16


gint get_xmms_session (void) {
    for( xmms_session = 0 ; xmms_session < MAX_XMMS_SESSION ; xmms_session++ )
        if ( xmms_remote_is_running( xmms_session ) )
            return xmms_session;
    return xmms_session;
}


void join_or_start_xmms ( void ) {
    unsigned int tries;
    /* Try to automatically find xmms session */
    get_xmms_session();
    
    if (xmms_session == MAX_XMMS_SESSION) {  /* no session found */
        switch( fork() ) {
        case -1:
            perror("fork");
            _exit(1);
            
        case 0:
            execlp("xmms", "xmms", NULL);
            fprintf(stderr, "xmms not found!\n");
            return;
            
        default:
            for( tries = 0 ; tries < 10 ; tries++ ) {
                usleep( 500000 ); /* in usec */
                if (get_xmms_session() < MAX_XMMS_SESSION)
                    return;
            }
            xmms_session = -1;
            return;
        }
    }
}


void play_song ( song * s ) {
    GList * list = NULL;
    join_or_start_xmms();
    if (xmms_session < 0)
        return;
    list = g_list_append(NULL, strdup_to_latin1(s->path));
    play_files(list);
    g_free((gchar *) list->data);
    g_list_free(list);
}


void play_songs ( GList * slist ) {
    GList * list = NULL, * ll;
    join_or_start_xmms();
    if (xmms_session < 0)
        return;
    for (; slist; slist = g_list_next(slist)) {
        list = g_list_append(list, strdup_to_latin1(SONG(slist)->path));
    } 
    play_files(list);
    for (ll = list; ll; ll = g_list_next(ll)) 
        g_free((gchar *) ll->data);
    g_list_free(list);
}


void play_files ( GList * list) {
    if (list) {
        xmms_remote_playlist_clear(xmms_session);
        xmms_remote_playlist_add(xmms_session, list);
        xmms_remote_play(xmms_session);
    }
}


song * get_current_xmms_song (void) {
    gchar * path;
    gint pos;
    song * s = NULL;
    join_or_start_xmms();
    if (xmms_session < MAX_XMMS_SESSION) {
        pos = xmms_remote_get_playlist_pos(xmms_session);
        path = xmms_remote_get_playlist_file(xmms_session, pos);    
        s = g_hash_table_lookup(song_name_hash, path);
        free(path);
    }
    return s;
}


gboolean xmms_is_running(void) {
    get_xmms_session();
    if ((xmms_session != -1) && (xmms_session < MAX_XMMS_SESSION)) {
        return TRUE;
    }
    return FALSE;
}

