/*
 * Gjay - Gtk+ DJ music playlist creator
 * Copyright (C) 2002-2004 Chuck Groom
 * Copyright (C) 2010-2015 Craig Small 
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

typedef struct SelectUI SelectUI;

SelectUI *selection_new(
        GjayApp *gjay);

void selection_destroy(
        SelectUI *sui);

GtkWidget *selection_get_widget(
        SelectUI *sui);

void selection_set_none(
        GjayApp *gjay);

void selection_set_file(
        GjayApp *gjay,
        const gchar *file,
        const char *short_name);

void selection_set_dir(
        GjayApp *gjay,
        const gchar *file,
        const char *short_name);

void selection_set_playall(
        GjayApp *gjay,
        const gboolean in_view);

void selection_set_rating_visible(
        SelectUI *sui,
        gboolean is_visible);

void selection_redraw(
        SelectUI *sui,
        GList *selected_songs);
