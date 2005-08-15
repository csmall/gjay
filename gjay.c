/**
 * GJay, copyright (c) 2002-2004 Chuck Groom
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
#include "gjay_xmms.h"
#include "analysis.h"
#include "ipc.h"
#include "playlist.h"
#include "vorbis.h"


gjay_mode mode; /* UI, DAEMON, PLAYLIST */

int daemon_pipe_fd;
int ui_pipe_fd;
gint verbosity;
gint skip_verify;

char * ogg_decoder_app = "ogg123";
char * mp3_decoder_app = "mpg321";
char * mp3_decoder_app_alternative =" mpg123";

static gboolean app_exists  ( gchar * app );
static void     kill_signal ( int sig );
static int      open_pipe   ( const char* filepath );
static gint     ping_daemon ( gpointer data );


int main( int argc, char *argv[] ) {
    GList * list;
    char buffer[BUFFER_SIZE];
    GtkWidget * widget;
    struct stat stat_buf;
    FILE * f;
    gint i, k, hex;
    gboolean m3u_format, playlist_in_xmms;

    srand(time(NULL));
    
    mode = UI;
    verbosity = 0;    
    skip_verify = 0;
    m3u_format = FALSE;
    playlist_in_xmms = FALSE;
    load_prefs();
    
    for (i = 0; i < argc; i++) {
        if ((strncmp(argv[i], "-h", 2) == 0) || 
            (strncmp(argv[i], "--help", 6) == 0)) {
            printf(HELP_TEXT);
            return 0;
        } else if (strncmp(argv[i], "--version", 9) == 0) {
            printf("GJay version %s\n", GJAY_VERSION);
            return 0;
        } else if (strncmp(argv[i], "-l", 2) == 0) {
            if (i + 1 < argc) {
                prefs.time = atoi(argv[i + 1]);
                i++;
            } else {
                fprintf(stderr, "Usage: -l length (in minutes)\n");
                return -1;
            }
        } else if (strncmp(argv[i], "-c", 2) == 0) {
            RGB rgb;
            if (i + 1 < argc) {
                strncpy(buffer, argv[i+1], BUFFER_SIZE);
                if (sscanf(buffer, "0x%x", &hex)) {
                    rgb.R = ((hex & 0xFF0000) >> 16) / 255.0;
                    rgb.G = ((hex & 0x00FF00) >> 8) / 255.0;
                    rgb.B = (hex & 0x0000FF) / 255.0;
                    prefs.start_color = TRUE;
                } else if (get_named_color(buffer, &rgb)) {
                    prefs.start_color = TRUE;
                }
                prefs.color = rgb_to_hsv(rgb);
                i++;
            } else {
                fprintf(stderr, "Usage: -c color, where color is a hex number in the form 0xRRGGBB or a color name:\n%s\n", known_colors());
                return -1;
            }            
        } else if (strncmp(argv[i], "-f", 2) == 0) {
            char fname[BUFFER_SIZE];
            char * path;
            if (i + 1 < argc) {
                strncpy(buffer, argv[i+1], BUFFER_SIZE);
                sscanf(buffer, "%s", fname);
                prefs.start_selected = TRUE;
                path = strdup_to_utf8(fname);
                selected_files = g_list_append(NULL, path);
                i++;
            } else {
                fprintf(stderr, "Usage: -f filename\n");
                return -1;
            }  
        } else if (argv[i][0] == '-') {
            for (k = 1; argv[i][k]; k++) {
                if (argv[i][k] == 'd') {
                    mode = DAEMON_DETACHED;
                    printf("Running as daemon. Ctrl+c to stop.\n");
                }
                if (argv[i][k] == 'v')
                    verbosity++;
                if (argv[i][k] == 's') {
                    printf("Skipping verification for debugging only.\n");
                    skip_verify = 1; 
                }
                if (argv[i][k] == 'u')
                    m3u_format = TRUE;
                if (argv[i][k] == 'x')
                    playlist_in_xmms = TRUE;
                if (argv[i][k] == 'p') {
                    prefs.start_color = FALSE;
                    mode = PLAYLIST;
                }
            }
        }
    }

    /* Check to see if we have all the apps we'll need for analysis */
    if (!app_exists(ogg_decoder_app)) {
        fprintf(stderr, "Sorry, GJay requires %s; quitting\n", 
                ogg_decoder_app); 
        return -1;
    }
    if (!app_exists(mp3_decoder_app)) {
        if (app_exists(mp3_decoder_app_alternative)) {
            mp3_decoder_app = mp3_decoder_app_alternative;
        } else {
            fprintf(stderr, "Sorry, GJay requires %s; quitting\n", 
                    mp3_decoder_app); 
            return -1;
        }
    }
    
    /* Make sure there is a "~/.gjay" directory */
    snprintf(buffer, BUFFER_SIZE, "%s/%s", getenv("HOME"), GJAY_DIR);
    if (stat(buffer, &stat_buf) < 0) {
        if (mkdir (buffer, 
                   S_IRUSR | S_IWUSR | S_IXUSR | S_IRGRP | S_IXGRP |
                   S_IROTH | S_IXOTH) < 0) {
            fprintf (stderr, "Could not create %s\n", buffer);
            perror(NULL);
            return 0;
        }
    }

    if (mode != PLAYLIST) {
        /* Create the named pipes for the daemon and ui processes to 
         * communicate through */
         
        /* Create a per-username directory for the daemon and UI pipes */
        char * login = getlogin();
        gjay_pipe_dir = (char *) malloc(sizeof(char) * (
            strlen(GJAY_PIPE_DIR_TEMPLATE) +
            strlen(login) + 1));
        sprintf(gjay_pipe_dir, "%s%s", GJAY_PIPE_DIR_TEMPLATE, login);

        /* Create directory if one doesn't already exist */
        struct stat buf;
        if (stat(gjay_pipe_dir, &buf) != 0) {
            if (mkdir(gjay_pipe_dir, 0700)) {
                fprintf(stderr, "Couldn't create %s\n", gjay_pipe_dir);
                return -1;
            }
        }

        /* Setup pipe names */
        ui_pipe = (char *) malloc(sizeof(char) * (
            strlen(gjay_pipe_dir) + 
            strlen(UI_PIPE_FILE)) + 2);
        daemon_pipe = (char *) malloc(sizeof(char) * (
            strlen(gjay_pipe_dir) + 
            strlen(DAEMON_PIPE_FILE)) + 2);
        sprintf(ui_pipe, "%s/%s", gjay_pipe_dir, UI_PIPE_FILE);
        sprintf(daemon_pipe, "%s/%s", gjay_pipe_dir, DAEMON_PIPE_FILE);

        /* Both daemon and UI app open an end of a pipe */
        ui_pipe_fd = open_pipe(ui_pipe);
        daemon_pipe_fd = open_pipe(daemon_pipe);
    }

    /* Try to load libvorbis; this is a soft dependancy */
    if (gjay_vorbis_dlopen() == 0) {
        printf("Ogg not supported; %s", gjay_vorbis_error());
    }
   
    if(mode == UI) {
        /* Make sure a daemon is running. If not, fork. */
        gboolean fork_daemon = FALSE;
        pid_t pid;
        
        snprintf(buffer, BUFFER_SIZE, "%s/%s/%s", 
                 getenv("HOME"), GJAY_DIR, GJAY_PID);
        f = fopen(buffer, "r");
        if (f) {
            fscanf(f, "%d", &i);
            fclose(f);
            snprintf(buffer, BUFFER_SIZE, "/proc/%d/stat", i);
            if (access(buffer, R_OK))
                fork_daemon = TRUE; 
        } else {
            fork_daemon = TRUE;
        }
        if (fork_daemon) {
            pid = fork();
            if (pid < 0) {
                fprintf(stderr, "Unable to fork daemon.\n");
            } else if (pid == 0) {
                /* Daemon */
                mode = DAEMON_INIT;
            }
        }
    }

    if ((mode == UI) || (mode == PLAYLIST)) {
        songs = NULL;
        not_songs = NULL;
        songs_dirty = FALSE;
        song_name_hash    = g_hash_table_new(g_str_hash, g_str_equal);
        song_inode_dev_hash = g_hash_table_new(g_int_hash, g_int_equal);
        not_song_hash     = g_hash_table_new(g_str_hash, g_str_equal);

        read_data_file();
   }

    if (mode == UI) {
        if (!app_exists("xmms")) {
            fprintf(stderr, "GJay strongly suggests xmms\n"); 
        } 

        gtk_init (&argc, &argv);
        
        g_io_add_watch (g_io_channel_unix_new (daemon_pipe_fd),
                        G_IO_IN,
                        daemon_pipe_input,
                        NULL);

        /* Ping the daemon ocassionally to let it know that the UI 
         * process is still around */
        gtk_timeout_add( UI_PING, ping_daemon, NULL);
        
        widget = make_app_ui();
        gtk_widget_show_all(widget);
        set_selected_rating_visible(prefs.use_ratings);
        set_playlist_rating_visible(prefs.use_ratings);
        set_add_files_progress_visible(FALSE);

        /* Periodically write song data to disk, if it has changed */
        gtk_timeout_add( SONG_DIRTY_WRITE_TIMEOUT, 
                         write_dirty_song_timeout, NULL);
                         
        send_ipc(ui_pipe_fd, ATTACH);
        if (skip_verify) {
            GList * llist;
            for (llist = g_list_first(songs); llist; llist = g_list_next(llist)) {
                SONG(llist)->in_tree = TRUE;
                SONG(llist)->access_ok = TRUE;
            }        
        } else {
            explore_view_set_root(prefs.song_root_dir);
        }        

        set_selected_file(NULL, NULL, FALSE);

        gtk_main();

        save_prefs();
        if (songs_dirty)
            write_data_file();

        if (prefs.detach || (prefs.daemon_action == PREF_DAEMON_DETACH)) {
            send_ipc(ui_pipe_fd, DETACH);
            send_ipc(ui_pipe_fd, UNLINK_DAEMON_FILE);
        } else {
            send_ipc(ui_pipe_fd, UNLINK_DAEMON_FILE);
            send_ipc(ui_pipe_fd, QUIT_IF_ATTACHED);
        }
  
        close(daemon_pipe_fd);
        close(ui_pipe_fd);
    } else if (mode == PLAYLIST) {
        /* Playlist mode */
        prefs.use_selected_songs = FALSE;
        prefs.rating_cutoff = FALSE;
        for (list = g_list_first(songs); list;  list = g_list_next(list)) {
            SONG(list)->in_tree = TRUE;
        }
        list = generate_playlist(prefs.time);
        if (playlist_in_xmms) {
            join_or_start_xmms();
            play_songs(list);
        } else {
            write_playlist(list, stdout, m3u_format);
        }
        g_list_free(list);
    } else {
        /* Daemon process */
        /* Write pid to ~/.gjay/gjay.pid */
        snprintf(buffer, BUFFER_SIZE, "%s/%s/%s", 
                 getenv("HOME"), GJAY_DIR, GJAY_PID);
        f = fopen(buffer, "w");
        if (f) {
            fprintf(f, "%d", getpid());
            fclose(f);
        } else {
            fprintf(stderr, "Unable to write to %s\n", GJAY_PID);
        }
        
        signal(SIGTERM, kill_signal);
        signal(SIGKILL, kill_signal);
        signal(SIGINT,  kill_signal);
        
        analysis_daemon();

        /* Daemon cleans up pipes on quit */
        close(daemon_pipe_fd);
        close(ui_pipe_fd);
        unlink(daemon_pipe);
        unlink(ui_pipe);
        rmdir(gjay_pipe_dir);
    }
    return(0);
}


static gboolean app_exists (gchar * app) {
    FILE * f;
    char buffer[BUFFER_SIZE];
    gboolean result = FALSE;

    snprintf(buffer, BUFFER_SIZE, "which %s", app); // Yes, I'm lame
    f = popen (buffer, "r");
    while (!feof(f) && fread(buffer, 1, BUFFER_SIZE, f)) 
        result = TRUE;
    pclose(f);
    return result;
}


/**
 * When the daemon receives a kill signal, delete ~/.gjay/gjay.pid
 */
static void kill_signal (int sig) {
    char buffer[BUFFER_SIZE];
    snprintf(buffer, BUFFER_SIZE, "%s/%s/%s", 
             getenv("HOME"), GJAY_DIR, GJAY_PID);
    unlink(buffer); 
    exit(0);
}


static int open_pipe(const char* filepath) {
    int fd = -1;

    if ((fd = open(filepath, O_RDWR)) < 0) {
        if (errno != ENOENT) {
            fprintf(stderr, "Error opening the pipe %s.\n", filepath);
            return -1;
        }

        if (mknod(filepath, S_IFIFO | 0777, 0)) {
            fprintf(stderr, "Couldn't create the pipe %s.\n", filepath);
            return -1;
        }

        if ((fd = open(filepath, O_RDWR)) < 0) {
            fprintf(stderr, "Couldn't open the pipe %s.\n", filepath);
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
        printf("Unable to convert from %s charset; perhaps encoded differently?", enc_from);
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
gchar * parent_dir ( const char * path ) {
    int len, rootlen;
    
    len = strlen(path);
    rootlen = strlen(prefs.song_root_dir);
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
