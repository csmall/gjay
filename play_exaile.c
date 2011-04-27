/**
 * Gjay - Gtk+ DJ music playlist creator
 * play_exaile.c : Output for exaile
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
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */

#ifdef WITH_EXAILE

#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include "gjay.h" 
#include "songs.h" 
#include "dbus.h"
#include "i18n.h"
#include "play_exaile.h"

static void exaile_connect(void);

/* public functions */

void
exaile_init(void)
{
  gjay->player_get_current_song = &exaile_get_current_song;
  gjay->player_is_running = &exaile_is_running;
  gjay->player_play_files = &exaile_play_files;
  gjay->player_start = &exaile_start;
}

gboolean
exaile_start(void)
{
  GError *error = NULL;
  GAppInfo *app;
  int i;

  if ((app = g_app_info_create_from_commandline(
    EXAILE_BIN,
    "exaile",
    G_APP_INFO_CREATE_NONE,
    &error)) == NULL)
  {
    g_warning(_("Cannot create exaile g_app: %s\n"), error->message);
    g_error_free(error);
    return FALSE;
  }
  error=NULL;
  if (g_app_info_launch(app, NULL, NULL, &error) == FALSE)
  {
    g_warning(_("Cannot launch exaile g_app: %s"), error->message);
    g_error_free(error);
    return FALSE;
  }
  /* Give it 3 tries */
  for(i=1; i<= 3; i++)
  {
    if (exaile_is_running())
      return TRUE;
    sleep(1);
  }
  return FALSE;
}

gboolean
exaile_is_running(void)
{
  return gjay_dbus_is_running(EXAILE_DBUS_INTERFACE);
}

song *
exaile_get_current_song(void)
{
  gchar buf[BUFSIZ];

  exaile_connect();
  
  exaile_dbus_query(gjay->player_proxy, "get_title", &buf);
  printf("title: %s\n", buf);
  g_warning("not implemented yet\n");
  return NULL;
  /*
  pos = exaile_remote_get_playlist_pos(gjay->player_proxy);
  playlist_file = exaile_remote_get_playlist_file(gjay->player_proxy, pos);
  if (playlist_file == NULL)
    return NULL;
  uri = g_filename_from_uri(playlist_file, NULL, NULL);
  if (uri == NULL)
    return NULL;
  s = g_hash_table_lookup(gjay->song_name_hash, uri);
  g_free(playlist_file);
  g_free(uri);
  return s;
  */
}

void
exaile_play_files ( GList *list) {

  if (!list)
    return;

  if (!exaile_is_running())
  {
    int i;
    GtkWidget *dialog;
    gint result;
    GError *error;

    dialog = gtk_message_dialog_new(GTK_WINDOW(gjay->main_window),
        GTK_DIALOG_DESTROY_WITH_PARENT,
        GTK_MESSAGE_QUESTION,
        GTK_BUTTONS_YES_NO,
        _("exaile is not running, start exaile?"));
    result = gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
    if (result == GTK_RESPONSE_YES)
    {
      error = NULL;
      GAppInfo *aud_app;

      if ((aud_app = g_app_info_create_from_commandline(
          EXAILE_BIN,
          "exaile",
          G_APP_INFO_CREATE_NONE,
          &error)) == NULL)
      {
        g_warning(_("Cannot create exaile g_app: %s\n"), error->message);
        g_error_free(error);
        return;
      }
      error=NULL;
      if (g_app_info_launch(aud_app, NULL, NULL, &error) == FALSE)
      {
        g_warning(_("Cannot launch exaile g_app: %s"), error->message);
        g_error_free(error);
        return;
      }
      /* Give it 3 tries */
      for(i=1; i<= 3; i++)
      {
        if (exaile_is_running())
          break;
        sleep(1);
      }
      if (i == 3) /* never got running */
      {
        gjay_error_dialog(_("Unable to start exaile"));
        return;
      }

    } else /* user clicked no */
      return;
  }
  exaile_connect();
  g_warning("not implemented\n");
  return;
  /*
  exaile_remote_stop(gjay->player_proxy);
  exaile_remote_playlist_clear(gjay->player_proxy);
  for (lptr=list; lptr; lptr = g_list_next(lptr)) {
      uri = g_filename_to_uri(lptr->data, NULL, NULL);
      exaile_remote_playlist_add_url_string(gjay->player_proxy, uri);
      g_free(uri);
  }
  exaile_remote_play(gjay->player_proxy);
  */
}

/* static functions */

/* Connect and init exaile instance */
static void
exaile_connect(void)
{
  if (gjay->player_proxy != NULL)
    return;

  gjay->player_proxy = dbus_g_proxy_new_for_name(gjay->connection,
      EXAILE_DBUS_SERVICE, EXAILE_DBUS_PATH, EXAILE_DBUS_INTERFACE);

}

gboolean
exaile_dbus_query(DBusGProxy *proxy, const char *method, char *dest)
{
  char *str = 0;
  GError *error = NULL;

  if (!dbus_g_proxy_call_with_timeout(proxy, method, 42, &error,
        G_TYPE_INVALID,
        G_TYPE_STRING, &str,
        G_TYPE_INVALID))
  {
    g_warning("Cannot proxy call to Exaile: %s\n", error->message);
    return FALSE;
  }

  assert(str);
  strncpy(dest, str, BUFSIZ);
  dest[BUFSIZ-1] = '\0';
  g_free(str);

  return TRUE;
}
        
#endif /* WITH_EXAILE */
