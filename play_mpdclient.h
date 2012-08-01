/*
 * Gjay - Gtk+ DJ music playlist creator
 * Copyright 2009-2012 Craig Small 
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

#ifndef _GJAY_MPDCLIENT_H
#define _GJAY_MPDCLIENT_H

#include "gjay.h" 

gboolean      mpdclient_init(GjayPlayer *player);
GjaySong*     mpdclient_get_current_song(GjayPlayer *player, GHashTable *htb);
gboolean  mpdclient_is_running(GjayPlayer *player);
void      mpdclient_play_files(GjayPlayer *player, GList *list);
gboolean mpdclient_start(GjayPlayer *player);

#endif /* _GJAY_MPD_H_ */
