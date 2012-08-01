/**
 * GJay, copyright (c) 2002-2004 Chuck Groom
 *       Copyright (c) 2010-2011 Craig Small
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
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
 *
 * Overview:
 *
 * GJay runs in interactive (UI), daemon, or playlist-generating mode.
 *
 * In UI mode, GJay creates playlists, displays analyzed
 * songs, and requests new songs for analysis.
 *
 * In daemon mode, GJay analyzes queued song requests. If the daemon runs in
 * 'unattached' mode, it will quit once the songs in the pending list are
 * finished.
 * 
 * In playlist mode, GJay prints a playlist and exits.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <sys/signal.h>
#include <stdlib.h>
#include <stdio.h> 
#include <sys/stat.h>
#include <sys/fcntl.h>
#include <sys/errno.h>
#include <pthread.h>
#include <string.h>
#include <ctype.h>
#include "gjay.h"

#ifdef WITH_DBUSGLIB
#include "dbus.h"
#endif /* WITH_DBUSGLIB */

#include "analysis.h"
#include "ipc.h"
#include "playlist.h"
#include "vorbis.h"
#include "flac.h"
#include "ui.h"
#include "play_common.h"
#include "i18n.h"



gboolean skip_verify;

//static gboolean app_exists  ( gchar * app );
//static void     kill_signal ( int sig );
static gboolean create_gjay_app(GjayApp **app);
static gint     ping_daemon ( gpointer data );
static gboolean mode_attached ( const gjay_mode m );
static void     fork_or_connect_to_daemon(gjay_mode *mode);
#ifdef WITH_GUI
static void     run_as_ui      (int argc, char * argv[], GjayApp *gjay);
#endif /* WITH_GUI */
static void     run_as_playlist  (GjayApp *gjay, guint playlist_minutes,
    gboolean m3u_format, 
    gboolean player_autostart );


static gboolean
print_version(const gchar *option_name, const gchar *value, gpointer data, GError **error)
{
  fprintf(stderr, _("GJay version %s\n"), VERSION);
  fprintf(stderr,
      "Copyright (C) 2002-2004 Chuck Groom\n"
      "Copyright (C) 2010-2011 Craig Small\n\n");
	fprintf(stderr,
		_("GJay comes with ABSOLUTELY NO WARRANTY.\n"
    "This program is free software; you can redistribute it and/or modify\n"
    "it under the terms of the GNU General Public License as published by\n"
    "the Free Software Foundation; either version 2 of the License, or\n"
    "(at your option) any later version.\n"));
  exit(0);
}

static void
parse_commandline(int *argc_p, char ***argv_p, GjayApp *gjay, guint *playlist_minutes, gboolean *m3u_format, gboolean *run_player, gchar **analyze_detached_fname, gjay_mode *mode)
{
  gboolean opt_daemon=FALSE, opt_playlist=FALSE;
  gchar *opt_standalone=NULL, *opt_color=NULL, *opt_file=NULL;
  GError *error;
  GOptionContext *context;

  GOptionEntry entries[] =
  {
    { "analyze-standalone", 'a', 0, G_OPTION_ARG_FILENAME, &opt_standalone, _("Analyze FILE and exit"), _("FILE") },
    { "color", 'c', 0, G_OPTION_ARG_STRING, &opt_color, _("Start playlist at color- Hex or name"), _("0xrrggbb|NAME") },
    { "daemon", 'd', 0, G_OPTION_ARG_NONE, &opt_daemon, _("Run as daemon"), NULL },
    { "file", 'f', 0, G_OPTION_ARG_STRING, &opt_file, _("Start playlist at file"), _("FILE") },
    { "length", 'l', 0, G_OPTION_ARG_INT, &playlist_minutes, _("Playlist length"), _("minutes") },
    { "playlist", 'p', 0, G_OPTION_ARG_NONE, &opt_playlist, _("Generate a playlist"), NULL },
    { "skip-verification", 's', 0, G_OPTION_ARG_NONE, &skip_verify, _("Skip file verification"), NULL },
    { "m3u-playlist", 'u', 0, G_OPTION_ARG_NONE, m3u_format, _("Use M3U playlist format"), NULL },
    { "verbose", 'v', 0, G_OPTION_ARG_INT, &(gjay->verbosity), "Set verbosity/debug level", _("LEVEL") },
    { "player-start", 'P', 0, G_OPTION_ARG_NONE, run_player, _("Start player using generated playlist"), NULL },
    { "version", 'V', G_OPTION_FLAG_NO_ARG, G_OPTION_ARG_CALLBACK , &print_version, _("Show version"), NULL },
    { NULL }
  };

  context = g_option_context_new(_("- Automatic DJ and playlist creator"));
  g_option_context_add_main_entries( context, entries, NULL);
/*  if (gtk_init_check(&argc, &argv))
  g_option_context_add_group (context, gtk_get_option_group (TRUE));*/
  error = NULL;
  if (!g_option_context_parse (context, argc_p, argv_p, &error))
  {
    g_print (_("option parsing failed: %s\n"), error->message);
    exit (1);
  }


  if (opt_standalone != NULL)
  {
    *analyze_detached_fname = opt_standalone;
    *mode = ANALYZE_DETACHED;
  }
  if (opt_color != NULL)
  {
    RGB rgb;
    gint hex;
    if (sscanf(opt_color, "0x%x", &hex))
    {
      rgb.R = ((hex & 0xFF0000) >> 16) / 255.0;
      rgb.G = ((hex & 0x00FF00) >> 8) / 255.0;
      rgb.B = (hex & 0x0000FF) / 255.0;
      gjay->prefs->use_color = TRUE;
      gjay->prefs->start_color = rgb_to_hsv(rgb);
    } else if (get_named_color(opt_color, &rgb))
    {
      gjay->prefs->use_color = TRUE;
      gjay->prefs->start_color = rgb_to_hsv(rgb);
    }
  }
  if (opt_daemon)
  {
    *mode = DAEMON_DETACHED;
    printf(_("Running as daemon. Ctrl+c to stop.\n"));
  }

  if (opt_file != NULL)
  {
    char *path;
    gjay->prefs->start_selected = TRUE;
    path = strdup_to_utf8(opt_file);
    gjay->selected_files = g_list_append(NULL, path);
  }
  if (opt_playlist) /* -p */
  {
    gjay->prefs->use_color = FALSE;
    *mode = PLAYLIST;
  }
}


int main( int argc, char *argv[] ) {
  GjayApp *gjay;
  gchar * analyze_detached_fname=NULL;
  gboolean m3u_format, player_autostart;
  guint playlist_minutes;
  gchar *gjay_home;
  gjay_mode mode; /* UI, DAEMON, PLAYLIST */

  srand(time(NULL));

  if (create_gjay_app(&gjay) == FALSE) {
	return 1;
  }
    

  mode = UI;
  skip_verify = 0;
  playlist_minutes = 0;
  m3u_format = FALSE;
  player_autostart = FALSE;
   
  parse_commandline(&argc, &argv, gjay, &playlist_minutes, &m3u_format, &player_autostart, &analyze_detached_fname, &mode);

  /* Make sure there is a "~/.gjay" directory */
 gjay_home = g_strdup_printf("%s/%s", g_get_home_dir(), GJAY_DIR);
 if (g_file_test(gjay_home, G_FILE_TEST_IS_DIR) == FALSE)
 {
   if (mkdir (gjay_home, 
         S_IRUSR | S_IWUSR | S_IXUSR | S_IRGRP | S_IXGRP |
         S_IROTH | S_IXOTH) < 0)
   {
     g_warning ( _("Could not create '%s'\n"), gjay_home);
     perror(NULL);
     return 0;
   }
  }
  g_free(gjay_home);

#ifdef HAVE_VORBIS_VORBISFILE_H
    /* Try to load libvorbis; this is a soft dependancy */
    if ( (gjay->ogg_supported = gjay_vorbis_dlopen()) == FALSE) 
#endif /* HAVE_VORBIS_VORBISFILE_H */
      if (gjay->verbosity)
        printf(_("Ogg not supported.\n"));

#ifdef HAVE_FLAC_METADATA_H
    if ( (gjay->flac_supported = gjay_flac_dlopen()) == FALSE)
#endif /* HAVE_FLAC_METADATA_H */
      if (gjay->verbosity)
        printf(_("FLAC not supported.\n"));

#ifdef WITH_GUI
    if (mode == UI) {
        /* UI needs a daemon */
        fork_or_connect_to_daemon(&mode);
    }
#endif /* WITH_GUI */

    switch(mode) {
#ifdef WITH_GUI
    case UI:
        read_data_file(gjay);
        sleep(1);
        run_as_ui(argc, argv, gjay);
        break;
#endif /* WITH_GUI */
    case PLAYLIST:
        read_data_file(gjay);
        run_as_playlist(gjay, playlist_minutes, m3u_format, player_autostart);
        break;
    case DAEMON_INIT:
    case DAEMON_DETACHED: 
        run_as_daemon(gjay, mode);
        break;
    case ANALYZE_DETACHED:
        run_as_analyze_detached(gjay, analyze_detached_fname);
        break;
    default:
        g_warning( _("Error: app mode %d not supported\n"), mode);
        return -1;
        break;
    }

    return(0);
}


/**
 * Read from the current file position to the end of the line ('\n'), 
 * including newline character
 */
void read_line ( FILE * f, char * buffer, int buffer_len) {
    int i;
    int result;

    for (i = 0; ((i < (buffer_len - 1)) &&
                 ((result = fgetc(f)) != EOF)); i++) {
        buffer[i] = (unsigned char) result;
        if (buffer[i] == '\n') {
            break;
        }
    }
    buffer[i] = '\0';
    return;
}



/**
 * We make sure to ping the daemon periodically such that it knows the
 * UI process is still attached. Otherwise, it will timeout after
 * about 20 seconds and quit.
 */
static gint ping_daemon ( gpointer data ) {
  int *fdp = (int*)data;
  send_ipc(*fdp, ACK);
  return TRUE;
}


/* Return true if the current mode attaches the daemon to the UI */
static gboolean mode_attached ( const gjay_mode m )
{
    switch(m) {
    case PLAYLIST:
    case ANALYZE_DETACHED:
        return FALSE;
    default:
        return TRUE;
    }
}


static gboolean
daemon_is_alive(void)
{
  gchar *path;
  FILE *fp;
  int pid;

  path = g_strdup_printf("%s/%s/%s", g_get_home_dir(),
      GJAY_DIR, GJAY_PID);
  fp = fopen(path, "r");
  g_free(path);

  if (fp == NULL)
    return FALSE;
  if (fscanf(fp, "%d", &pid) != 1)
  {
    fclose(fp);
    return FALSE;
  }
  fclose(fp);
  path = g_strdup_printf("/proc/%d/stat", pid);
  if (access(path, R_OK) != 0)
  {
    g_free(path);
    return FALSE;
  }
  g_free(path);
  return TRUE;
}


/* Fork a new daemon if one isn't running */
static void
fork_or_connect_to_daemon(gjay_mode *mode)
{
  pid_t pid;
  /* Make sure a daemon is running. If not, fork. */

  if (daemon_is_alive() == FALSE)
  {
    pid = fork();
    if (pid < 0) {
      g_warning(_("Unable to fork daemon.\n"));
    } else if (pid == 0) {
      /* Daemon child */
      *mode = DAEMON_INIT;
    }
  }
}

#ifdef WITH_GUI

static void run_as_ui(int argc, char *argv[], GjayApp *gjay ) 
{    

  if (gtk_init_check(&argc, &argv) == FALSE)
  {
    g_warning(_("Cannot initialise Gtk.\n"));
    exit(1);
  }
  	if (create_gjay_ipc(&(gjay->ipc)) == FALSE)
  	{
		g_error(_("Cannot create IPC pipes.\n"));
		exit(1);
  	}
  	g_io_add_watch (g_io_channel_unix_new (gjay->ipc->daemon_fifo),
                  G_IO_IN,
                  daemon_pipe_input,
                  gjay);
  	/* Ping the daemon ocassionally to let it know that the UI 
   	* process is still around */
  	gtk_timeout_add( UI_PING, ping_daemon, &(gjay->ipc->daemon_fifo));

  create_player(&(gjay->player), gjay->prefs->music_player);

  if (create_gjay_gui(gjay) == FALSE) {
	g_error(_("Cannot create GUI.\n"));
	exit(1);
  }
  gjay->player->main_window = gjay->gui->main_window;
  gjay->player->song_root_dir = gjay->prefs->song_root_dir;

  gtk_signal_connect (GTK_OBJECT (gjay->gui->main_window), "delete_event",
	  GTK_SIGNAL_FUNC (quit_app), gjay);



    set_selected_rating_visible(gjay->prefs->use_ratings);
    set_playlist_rating_visible(gjay->prefs->use_ratings);
    set_add_files_progress_visible(FALSE);
    
    /* Periodically write song data to disk, if it has changed */
    gtk_timeout_add( SONG_DIRTY_WRITE_TIMEOUT, 
                     write_dirty_song_timeout, gjay);
    
    send_ipc(gjay->ipc->ui_fifo, ATTACH);
    if (skip_verify) {
        GList * llist;
        for (llist = g_list_first(gjay->songs->songs); llist; llist = g_list_next(llist)) {
            SONG(llist)->in_tree = TRUE;
            SONG(llist)->access_ok = TRUE;
        }        
    } else {
        explore_view_set_root(gjay);
    }        
    
    set_selected_file(gjay, NULL, NULL, FALSE);
    /*gjay->player_is_running();*/
    gtk_main();
    
    save_prefs(gjay->prefs);
    if (gjay->songs->dirty)
        write_data_file(gjay);
    
    if (gjay->prefs->detach || (gjay->prefs->daemon_action == PREF_DAEMON_DETACH)) {
        send_ipc(gjay->ipc->ui_fifo, DETACH);
        send_ipc(gjay->ipc->ui_fifo, UNLINK_DAEMON_FILE);
    } else {
        send_ipc(gjay->ipc->ui_fifo, UNLINK_DAEMON_FILE);
        send_ipc(gjay->ipc->ui_fifo, QUIT_IF_ATTACHED);
    }
    
	destroy_gjay_ipc(gjay->ipc);
}
#endif /* WITH_GUI */


/* Playlist mode */
static void run_as_playlist(GjayApp *gjay, guint playlist_minutes, gboolean m3u_format, gboolean player_autostart)
{
    GList * list;
    gjay->prefs->use_selected_songs = FALSE;
    gjay->prefs->rating_cutoff = FALSE;
    for (list = g_list_first(gjay->songs->songs); list;  list = g_list_next(list)) {
        SONG(list)->in_tree = TRUE;
    }
    if (playlist_minutes == 0)
      playlist_minutes = gjay->prefs->playlist_time;
    list = generate_playlist(gjay, playlist_minutes);
    if (player_autostart) {
#ifdef WITH_GUI
        play_songs(gjay->player, gjay->gui->main_window, list);
#else
        play_songs(gjay->player, NULL, list);
#endif /* WITH_GUI */
    } else {
        write_playlist(list, stdout, m3u_format);
    }
    g_list_free(list);
}



static gboolean
create_gjay_app(GjayApp **app) {

  if ((*app = g_malloc0(sizeof(GjayApp))) == NULL) {
    g_warning( _("Unable to allocate memory for app.\n"));
	return FALSE;
  }
  (*app)->gui = NULL;
  (*app)->ipc = NULL;
  (*app)->verbosity = 0;
  (*app)->prefs = load_prefs();
  (*app)->player = NULL;
  return TRUE;
}
