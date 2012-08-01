/*
 * Gjay - Gtk+ DJ music playlist creator
 * Copyright (C) 2009-2011 Craig Small 
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

#ifndef _GJAY_AUDACIOUS_H_
#define _GJAY_AUDACIOUS_H_

#include "gjay.h" 

gboolean      audacious_init(GjayPlayer *player);
GjaySong*     audacious_get_current_song(GjayPlayer *player, GHashTable *song_name_hash);
gboolean  audacious_is_running(GjayPlayer *player);
void      audacious_play_files(GjayPlayer *player, GList *list);
gboolean audacious_start(GjayPlayer *player);

#endif /* _GJAY_AUDACIOUS_H_ */
