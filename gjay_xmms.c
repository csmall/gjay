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

static gint xmms_session = -1;

void init_xmms ( void ) {
    unsigned int tries;
    /* Try to automatically find xmms session */
    for( xmms_session = 0 ; xmms_session < 16 ; xmms_session++ )
        if ( xmms_remote_is_running( xmms_session ) )
            return;
    
    if (xmms_session == 16) {  /* no session found */
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
                for( xmms_session = 0 ; xmms_session < 16 ; xmms_session++ )
                    if ( xmms_remote_is_running( xmms_session ) )
                        return;
            }
            xmms_session = -1;
            return;
        }
    }
}


void play_song ( song * s ) {
    GList * list = NULL;
    init_xmms();
    if (xmms_session < 0)
        return;
    list = g_list_append(NULL, s->path);
    xmms_remote_playlist_clear(xmms_session);
    xmms_remote_playlist_add(xmms_session, list);
    xmms_remote_play(xmms_session);
    g_list_free(list);
}

void play_songs ( GList * slist ) {
    GList * list = NULL;
    init_xmms();
    if (xmms_session < 0)
        return;
    for (; slist; slist = g_list_next(slist)) {
        list = g_list_append(list, SONG(slist)->path);
    } 
    xmms_remote_playlist_clear(xmms_session);
    xmms_remote_playlist_add(xmms_session, list);
    xmms_remote_play(xmms_session);
    g_list_free(list);
}
