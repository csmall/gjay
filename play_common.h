/*
 * Gjay - Gtk+ DJ music playlist creator
 * Copyright 2010-2012 Craig Small 
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

#ifndef PLAY_COMMON_H
#define PLAY_COMMON_H

gboolean
create_player(GjayPlayer **player, const gushort music_player);
void play_song(GjayApp *gjay, GjaySong *s);
#ifdef WITH_GUI
void play_songs (GjayPlayer *player, GtkWidget *main_window, GList *slist);
#else
void play_songs (GjayPlayer *player, gpointer dummy, GList *slist);
#endif /* WITH_GUI */
void set_player_name(GjayPlayer *player, const gushort selected_player);

#endif /* PLAY_COMMON_H */
