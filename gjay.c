/**
 * GJay, copyright (c) 2002-2004 Chuck Groom
 *       Copyright (c) 2010 Craig Small
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
#include "gjay_audacious.h"
#include "dbus.h"
#include "analysis.h"
#include "ipc.h"
#include "playlist.h"
#include "vorbis.h"
#include "ui.h"
#include "gjay_audacious.h"
#include "i18n.h"

GjayApp *gjay;

gjay_mode mode; /* UI, DAEMON, PLAYLIST */

int daemon_pipe_fd;
int ui_pipe_fd;
gboolean verbosity;
gboolean skip_verify;

//static gboolean app_exists  ( gchar * app );
//static void     kill_signal ( int sig );
static int      open_pipe   ( const char* filepath );
static gint     ping_daemon ( gpointer data );
static gboolean create_ui_daemon_pipe(void);
static gboolean mode_attached ( gjay_mode m );
static void     fork_or_connect_to_daemon(void);
static void     run_as_ui      (int argc, char * argv[]);
static void     run_as_playlist  ( gboolean m3u_format, 
                                   gboolean playlist_in_audacious );

static gboolean
print_version(const gchar *option_name, const gchar *value, gpointer data, GError **error)
{
  fprintf(stderr, _("GJay version %s\n"), VERSION);
  fprintf(stderr,
      "Copyright (C) 2002-2004 Chuck Groom\n"
      "Copyright (C) 2010 Craig Small\n\n");
	fprintf(stderr,
		_("GJay comes with ABSOLUTELY NO WARRANTY.\n"
    "This program is free software; you can redistribute it and/or modify\n"
    "it under the terms of the GNU General Public License as published by\n"
    "the Free Software Foundation; either version 2 of the License, or\n"
    "(at your option) any later version.\n"));
  exit(0);
}

gboolean
verbose_option_cb(GOptionContext *context, GOptionGroup *group,
    gpointer data, GError **error)
{
  if (data == NULL) {
    verbosity=1;
    return TRUE;
  }
  verbosity = strtol((char*)data, NULL, 10);
  if (verbosity < 1 || verbosity > 10)
    verbosity=1;
  return TRUE;
}

static void
parse_commandline(int *argc_p, char ***argv_p, gboolean *m3u_format, gboolean *run_player, gchar **analyze_detached_fname)
{
  gboolean opt_daemon=FALSE, opt_playlist=FALSE;
  gint opt_length=0;
  gchar *opt_standalone=NULL, *opt_color=NULL, *opt_file=NULL;
  GError *error;
  GOptionContext *context;

  GOptionEntry entries[] =
  {
    { "analyze-standalone", 'a', 0, G_OPTION_ARG_FILENAME, &opt_standalone, "Analyze just FILE as standalone", "FILE" },
    { "color", 'c', 0, G_OPTION_ARG_STRING, &opt_color, "Start playlist at color- Hex or name", "0xrrggbb|NAME" },
    { "daemon", 'd', 0, G_OPTION_ARG_NONE, &opt_daemon, "Run as daemon", NULL },
    { "file", 'f', 0, G_OPTION_ARG_STRING, &opt_file, "Start playlist at file", NULL },
    { "length", 'l', 0, G_OPTION_ARG_INT, &opt_length, "Playlist length", NULL },
    { "playlist", 'p', 0, G_OPTION_ARG_NONE, &opt_playlist, "Generate a playlist", NULL },
    { "skip-verification", 's', 0, G_OPTION_ARG_NONE, &skip_verify, "Skip file verification", NULL },
    { "m3u-playlist", 'u', 0, G_OPTION_ARG_NONE, m3u_format, "Use M3U playlist format", NULL },
    { "verbose", 'v', G_OPTION_FLAG_OPTIONAL_ARG, G_OPTION_ARG_INT, &verbosity, "Verbose", "Verbosity Level" },
    { "play-audacious", 'P', 0, G_OPTION_ARG_NONE, run_player, "Play generated playlist in Audacious", NULL },
    { "version", 'V', G_OPTION_FLAG_NO_ARG, G_OPTION_ARG_CALLBACK , &print_version, "Show version", NULL },
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
    mode = ANALYZE_DETACHED;
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
      gjay->prefs->start_color = TRUE;
      gjay->prefs->color = rgb_to_hsv(rgb);
    } else if (get_named_color(opt_color, &rgb))
    {
      gjay->prefs->start_color = TRUE;
      gjay->prefs->color = rgb_to_hsv(rgb);
    }
  }
  if (opt_daemon)
  {
    mode = DAEMON_DETACHED;
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
    gjay->prefs->start_color = FALSE;
    mode = PLAYLIST;
  }
}


int main( int argc, char *argv[] ) {
  gchar * analyze_detached_fname=NULL;
  gboolean m3u_format, playlist_in_audacious;
  gchar *gjay_home;

  srand(time(NULL));
    
  if ((gjay = g_malloc0(sizeof(GjayApp))) == NULL) {
    fprintf(stderr, _("Unable to allocate memory for app.\n"));
    exit(1);
  }

  mode = UI;
  verbosity = 0;    
  skip_verify = 0;
  m3u_format = FALSE;
  playlist_in_audacious = FALSE;
  gjay->prefs = load_prefs();
   
  parse_commandline(&argc, &argv, &m3u_format, &playlist_in_audacious, &analyze_detached_fname);

    /* Intialize vars */
    daemon_pipe_fd = -1;
    ui_pipe_fd = -1;
    gjay->songs = NULL;
    gjay->not_songs = NULL;
    gjay->songs_dirty = FALSE;
    gjay->song_name_hash    = g_hash_table_new(g_str_hash, g_str_equal);
    gjay->song_inode_dev_hash = g_hash_table_new(g_int_hash, g_int_equal);
    gjay->not_song_hash     = g_hash_table_new(g_str_hash, g_str_equal);

    /* Check to see if we have all the apps we'll need for analysis */
  if ( g_find_program_in_path(OGG_DECODER_APP) == NULL)
  {
    fprintf(stderr, _("Sorry, GJay requires %s; quitting\n"), OGG_DECODER_APP); 
    return -1;
 }
 if ( (g_find_program_in_path(MP3_DECODER_APP1) == NULL) &&
   (g_find_program_in_path(MP3_DECODER_APP2) == NULL))
 {
     fprintf(stderr, _("Sorry, GJay requires %s; quitting\n"), 
                    MP3_DECODER_APP1); 
     return -1;
 }
    
  /* Make sure there is a "~/.gjay" directory */
 gjay_home = g_strdup_printf("%s/%s", g_get_home_dir(), GJAY_DIR);
 if (g_file_test(gjay_home, G_FILE_TEST_IS_DIR) == FALSE)
 {
   if (mkdir (gjay_home, 
         S_IRUSR | S_IWUSR | S_IXUSR | S_IRGRP | S_IXGRP |
         S_IROTH | S_IXOTH) < 0)
   {
     fprintf (stderr, _("Could not create %s\n"), gjay_home);
     perror(NULL);
     return 0;
   }
  }
  g_free(gjay_home);

    if (mode_attached(mode)) {
        if (create_ui_daemon_pipe() == FALSE) {
            return -1;
        }
    }

    /* Try to load libvorbis; this is a soft dependancy */
    if (gjay_vorbis_dlopen() == 0) {
        printf(_("Ogg not supported; %s"), gjay_vorbis_error());
    }
    if (mode == UI) {
        /* UI needs a daemon */
        fork_or_connect_to_daemon();
    }

    switch(mode) {
    case UI:
        read_data_file();
        sleep(1);
        run_as_ui(argc, argv);
        break;
    case PLAYLIST:
        read_data_file();
        run_as_playlist(m3u_format, playlist_in_audacious);
        break;
    case DAEMON_INIT:
    case DAEMON_DETACHED: 
        run_as_daemon();
        break;
    case ANALYZE_DETACHED:
        run_as_analyze_detached(analyze_detached_fname);
        break;
    default:
        fprintf(stderr, _("Error: app mode %d not supported\n"), mode);
        return -1;
        break;
    }

    return(0);
}


static int open_pipe(const char* filepath) {
    int fd = -1;

    if ((fd = open(filepath, O_RDWR)) < 0) {
        if (errno != ENOENT) {
            fprintf(stderr, ("Error opening the pipe %s.\n"), filepath);
            return -1;
        }

        if (mknod(filepath, S_IFIFO | 0777, 0)) {
            fprintf(stderr, _("Couldn't create the pipe %s.\n"), filepath);
            return -1;
        }

        if ((fd = open(filepath, O_RDWR)) < 0) {
            fprintf(stderr, _("Couldn't open the pipe %s.\n"), filepath);
            return -1;
        }
    }
    return fd;
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
 * Duplicate a string from one encoding to another
 */
gchar * strdup_convert ( const gchar * str, 
                         const gchar * enc_to, 
                         const gchar * enc_from ) {
    gchar * conv;
    gsize b_read, b_written;
    conv = g_convert (str,
                      -1, 
                      enc_to,
                      enc_from,
                      &b_read,
                      &b_written,
                      NULL);
    if (!conv) {
        printf(_("Unable to convert from %s charset; perhaps encoded differently?"), enc_from);
        return g_strdup(str);
    }
    return conv;
}


/**
 * Implement strtof, except make it locale agnostic w/r/t whether a
 * decimal is "," or "."
 */
float strtof_gjay ( const char *nptr, char **endptr) {
    char * end;
    float base = 10.0;
    float divisor;
    float result = 0;

    result = strtol(nptr, &end, 10);
    if ((*end == '.') || (*end == ',')) {
        end++;
        divisor = base;
        while (isdigit(*end)) {
            result += (*end - '0') / divisor;
            divisor *= base;
            end++;
        }
    }
    if (endptr) 
        *endptr = end;
    return result;
}


/**
 * Get the parent directory of path. The returned value should be freed.
 * If the parent is above root, return NULL.
 */
gchar * parent_dir (const char * path ) {
    int len, rootlen;
    
    len = strlen(path);
    rootlen = strlen(gjay->prefs->song_root_dir);
    if (len <= rootlen)
        return NULL;
    if (!len)
        return NULL;
    for (; len > rootlen; len--) {
        if (path[len] == '/') {
            return g_strndup(path, len);
        }
    }
    if (path[len - 1] == '/')
        len--;
    return g_strndup(path, len);
}


/**
 * We make sure to ping the daemon periodically such that it knows the
 * UI process is still attached. Otherwise, it will timeout after
 * about 20 seconds and quit.
 */
static gint ping_daemon ( gpointer data ) {
    send_ipc(ui_pipe_fd, ACK);
    return TRUE;
}


/**
 * Create the named pipes for the daemon and ui processes to 
 * communicate through. Return true on success. */
static gboolean create_ui_daemon_pipe(void)
{
  /* Create a per-username directory for the daemon and UI pipes */
  const gchar *username;

  if ( (username = g_get_user_name()) == NULL)
    return FALSE;

  gjay_pipe_dir =  g_strdup_printf("%s/gjay-%s",
      g_get_tmp_dir(), username);
  /* Create directory if one doesn't already exist */
  struct stat buf;
  if (stat(gjay_pipe_dir, &buf) != 0) {
    if (mkdir(gjay_pipe_dir, 0700)) {
        fprintf(stderr, _("Couldn't create %s\n"), gjay_pipe_dir);
        return FALSE;
    }
  }
    
  /* Setup pipe names */
  ui_pipe = g_strdup_printf("%s/%s", gjay_pipe_dir, UI_PIPE_FILE);
  daemon_pipe = g_strdup_printf("%s/%s", gjay_pipe_dir, DAEMON_PIPE_FILE);
    
  /* Both daemon and UI app open an end of a pipe */
  ui_pipe_fd = open_pipe(ui_pipe);
  daemon_pipe_fd = open_pipe(daemon_pipe);

  return TRUE;
}

/* Return true if the current mode attaches the daemon to the UI */
static gboolean mode_attached ( gjay_mode m )
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
fork_or_connect_to_daemon(void)
{
  pid_t pid;
  /* Make sure a daemon is running. If not, fork. */

  if (daemon_is_alive() == FALSE)
  {
    pid = fork();
    if (pid < 0) {
      fprintf(stderr, _("Unable to fork daemon.\n"));
    } else if (pid == 0) {
      /* Daemon child */
      mode = DAEMON_INIT;
    }
  }
}


static void run_as_ui(int argc, char *argv[] ) 
{    

  if (gtk_init_check(&argc, &argv) == FALSE)
  {
    g_print(_("Cannot initialise Gtk.\n"));
    exit(1);
  }
  g_io_add_watch (g_io_channel_unix_new (daemon_pipe_fd),
                  G_IO_IN,
                  daemon_pipe_input,
                  NULL);
  /* Ping the daemon ocassionally to let it know that the UI 
   * process is still around */
  gtk_timeout_add( UI_PING, ping_daemon, NULL);
    make_app_ui();
    gtk_widget_show_all(gjay->main_window);
    gjay->connection = gjay_dbus_connection();

    set_selected_rating_visible(gjay->prefs->use_ratings);
    set_playlist_rating_visible(gjay->prefs->use_ratings);
    set_add_files_progress_visible(FALSE);
    
    /* Periodically write song data to disk, if it has changed */
    gtk_timeout_add( SONG_DIRTY_WRITE_TIMEOUT, 
                     write_dirty_song_timeout, NULL);
    
    send_ipc(ui_pipe_fd, ATTACH);
    if (skip_verify) {
        GList * llist;
        for (llist = g_list_first(gjay->songs); llist; llist = g_list_next(llist)) {
            SONG(llist)->in_tree = TRUE;
            SONG(llist)->access_ok = TRUE;
        }        
    } else {
        explore_view_set_root(gjay->prefs->song_root_dir);
    }        
    
    set_selected_file(NULL, NULL, FALSE);
    audacious_is_running();
    gtk_main();
    
    save_prefs();
    if (gjay->songs_dirty)
        write_data_file();
    
    if (gjay->prefs->detach || (gjay->prefs->daemon_action == PREF_DAEMON_DETACH)) {
        send_ipc(ui_pipe_fd, DETACH);
        send_ipc(ui_pipe_fd, UNLINK_DAEMON_FILE);
    } else {
        send_ipc(ui_pipe_fd, UNLINK_DAEMON_FILE);
        send_ipc(ui_pipe_fd, QUIT_IF_ATTACHED);
    }
    
    close(daemon_pipe_fd);
    close(ui_pipe_fd);
}


/* Playlist mode */
static void run_as_playlist(gboolean m3u_format, gboolean playlist_in_audacious)
{
    GList * list;
    gjay->prefs->use_selected_songs = FALSE;
    gjay->prefs->rating_cutoff = FALSE;
    for (list = g_list_first(gjay->songs); list;  list = g_list_next(list)) {
        SONG(list)->in_tree = TRUE;
    }
    list = generate_playlist(gjay->prefs->time);
    if (playlist_in_audacious) {
        play_songs(list);
    } else {
        write_playlist(list, stdout, m3u_format);
    }
    g_list_free(list);
}


