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
 *
 * USAGE: gjay [--help] [-h] [-d] [-v [-v]]
 *
 *  --help, -h  :  Display help
 *  -d          :  Run as daemon (unattched)
 *  -v          :  Run in verbose mode. -v -v for lots more info.
 *
 * Explanation: 
 *
 * GJay runs in either interactive (UI) or daemon mode.
 *
 * In UI mode, GJay creates playlists, displays analyzed
 * songs, and requests new songs for analysis.
 *
 * In daemon mode, GJay analyzes queued song requests. If the daemon runs in
 * 'unattached' mode, it will quit once the songs in the pending list are
 * finished.
 * 
 *
 */

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h> 
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/signal.h>
#include <sys/unistd.h>
#include <sys/fcntl.h>
#include <sys/errno.h>
#include <pthread.h>
#include <string.h>
#include <locale.h>
#include "gjay.h"
#include "analysis.h"
#include "ipc.h"


#define NUM_APPS 3
static gchar * apps[NUM_APPS] = {
    "mpg321",
    "ogg123",
    "mp3info"
};


/* Application mode -- UI, DAEMON... */
gjay_mode mode;


static gboolean app_exists (gchar * app);
static void kill_signal(int sig);
static int open_pipe(const char* filepath);

int daemon_pipe_fd;
int ui_pipe_fd;

int verbosity;


int main( int argc, char *argv[] ) 
{
    char buffer[BUFFER_SIZE];
    GtkWidget * widget;
    struct stat stat_buf;
    FILE * f;
    gint i;

    /* Use '.' as decimal separator */
    setlocale(LC_NUMERIC, "C");

    mode = UI;
    verbosity = 0;
    
    for (i = 0; i < argc; i++) {
        if ((strncmp(argv[i], "-h", 2) == 0) || 
            (strncmp(argv[i], "--help", 6) == 0)) {
            printf("USAGE: gjay [--help] [-h] [-d] [-v [-v]]\n" \
                   "\t--help, -h  :  Display this help message\n" \
                   "\t-d          :  Run as daemon\n" \
                   "\t-v -v       :  Verbose mode. Repeat for way too much text\n");
            return 0;
        }
        if (strncmp(argv[i], "-d", 2) == 0) {
            mode = DAEMON_DETACHED;
        }
        if (strncmp(argv[i], "-v", 2) == 0) {
            verbosity++;
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

    /* Both daemon and UI app open an end of a pipe */
    daemon_pipe_fd = open_pipe(DAEMON_PIPE);
    ui_pipe_fd = open_pipe(UI_PIPE);

    if(mode == UI) {
        if (!app_exists("xmms")) {
            fprintf(stderr, "GJay strongly suggests xmms\n"); 
        } 

        srand(time(NULL));
        init_xmms();
        gtk_init (&argc, &argv);
        
        g_io_add_watch (g_io_channel_unix_new (daemon_pipe_fd),
                        G_IO_IN,
                        daemon_pipe_input,
                        NULL);

        songs = NULL;
        rated = NULL;
        files_not_song = NULL;
        song_name_hash      = g_hash_table_new(g_str_hash, g_str_equal);
        rated_name_hash     = g_hash_table_new(g_str_hash, g_str_equal);
        files_not_song_hash = g_hash_table_new(g_str_hash, g_str_equal);
        
        load_prefs();
        read_data_file();
        read_attr_file();

        widget = make_app_ui();
        gtk_widget_show_all(widget);

        send_ipc(ui_pipe_fd, ATTACH);
        set_selected_file(NULL, NULL, FALSE);

        gtk_main();

        save_prefs();
        write_data_file();
        write_attr_file();
        if (prefs.detach || (prefs.daemon_action == PREF_DAEMON_DETACH)) {
            send_ipc(ui_pipe_fd, DETACH);
            send_ipc(ui_pipe_fd, UNLINK_DAEMON_FILE);
        } else {
            send_ipc(ui_pipe_fd, UNLINK_DAEMON_FILE);
            send_ipc(ui_pipe_fd, QUIT_IF_ATTACHED);
        }
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
        
        /* Check to see if we have all the apps we'll need for analysis */
        for (i = 0; i < NUM_APPS; i++) {
            if (!app_exists(apps[i])) {
                fprintf(stderr, "GJay requires %s\n", apps[i]); 
                return -1;
            } 
        } 
        analysis_daemon();
    }

    if (daemon_pipe_fd > 0)
        close(daemon_pipe_fd);
    if (ui_pipe_fd > 0)
        close(ui_pipe_fd);

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

