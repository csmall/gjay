/*
 * Gjay - Gtk+ DJ music playlist creator
 * Copyright (C) 2010 Craig Small 
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

#include "gjay.h" 
#include "play_audacious.h"

void
player_init(void)
{
  gjay->player_get_current_song = &audacious_get_current_song;
  gjay->player_is_running = &audacious_is_running;
  gjay->player_play_files = &audacious_play_files;
}

void
play_song(song *s)
{
  GList *list;

  list = g_list_append(NULL, strdup_to_latin1(s->path));
  gjay->player_play_files(list);
  g_free((gchar*)list->data);
  g_list_free(list);
}

void play_songs (GList *slist) {
  GList *list = NULL;
  
  for (; slist; slist = g_list_next(slist)) 
    list = g_list_append(list, strdup_to_latin1(SONG(slist)->path));
  gjay->player_play_files(list);
}

