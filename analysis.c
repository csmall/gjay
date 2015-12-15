/*
 * Gjay - Gtk+ DJ music playlist creator
 * Copyright (C) 2010-2012 Craig Small 
 * Copyright (C) 2002 Chuck Groom.
 *
 * Sections of this code come from spectromatic, copyright (C)
 * 1997-2002 Daniel Franklin, and BpmDJ by Werner Van Belle.
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
 * with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * Analysis.c -- manages the background threads and processes involved
 * in song analysis.
 */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/resource.h>
#include <errno.h>
#include <string.h>
#include <sys/poll.h>
#include <endian.h>
#include <math.h>
#include <time.h>
#include <assert.h>
#include <math.h>
#include <gsl/gsl_errno.h>
#include <gsl/gsl_fft_real.h>
#include <gsl/gsl_fft_halfcomplex.h>
#include "gjay.h"
#include "analysis.h"
#include "ipc.h"
#include "i18n.h"

#define BPM_BUF_SIZE    32*1024
#define SHARED_BUF_SIZE sizeof(short int) * BPM_BUF_SIZE
#define SLEEP_WHILE_IDLE 500

#define BROKEN_VAL 100000000000000.0

/* Multiplier for the frequency magnitude. 2.0 is close to the orginal value
 * 5.0 gives a brighter display
 */
#define MAGS_MULTIPLIER 2.0

typedef struct {
    FILE * f;
    waveheaderstruct header;
    long int freq_seek, seek;
    char buffer[SHARED_BUF_SIZE];
} wav_file;

/* Sturcture to ferry data around */
struct daemon_data {
  guint			verbosity;
  gboolean		ogg_supported;
  gboolean		flac_supported;
  gjay_mode		mode;

  GjayIPC		*ipc;
  GMainLoop   * loop;
  time_t        last_ping;
  gboolean		in_analysis;
  GjaySong      *analyze_song;

  gchar * mp3_decoder;
  gchar * ogg_decoder;
  gchar * flac_decoder;
};


#define AUDIO_RATE 2756UL      /* Author states that 11025 is perfect
                                  measure, but we can tolerate more
                                  lossiness for the sake of speed */
#define START_BPM 120UL
#define STOP_BPM 160UL


/* Spectrum globals */
#define MAX_FREQ 22500
#define START_FREQ 100

#define WINDOW_SIZE 1024
#define STEP_SIZE 1024

static char       * read_buffer;
static int          read_buffer_size;
static long         read_buffer_start;
static long         read_buffer_end; /* same as file position */
static GList      * queue = NULL; 
static GHashTable * queue_hash = NULL;

static FILE *     inflate_to_wav (struct daemon_data *ddata,
							  const gchar * path, 
                              const song_file_type type);

static int           run_analysis     ( struct daemon_data *ddata,
								 wav_file * wsfile,
                                 gdouble * freq_results,
                                 gdouble * volume_diff,
                                 gdouble * bpm_result );
static unsigned long bpm_phasefit     ( const long i,
    const unsigned char const *audio,
    const unsigned long audiosize);
static int           freq_read_frames ( wav_file * wsfile, 
                                 int start, 
                                 int length, 
                                 void *data );
static void          send_ui_percent        ( const int fd, int percent );
static void          send_analyze_song_name ( const int fd, GjaySong *song; );
static void   write_queue       ( void );
static void kill_signal (int sig);
static void analysis_daemon(struct daemon_data *data);
gboolean      daemon_idle       ( gpointer data );
static void analyze( struct daemon_data *data, const char * fname, const gboolean result_to_stdout);
static struct daemon_data *create_daemon_data(GjayApp *gjay);
gboolean ui_pipe_input (GIOChannel *source,
                        GIOCondition condition,
                        gpointer data);

void
run_as_analyze_detached  (GjayApp *gjay,  const char * analyze_detached_fname)
{
  struct daemon_data *data;

  if (access(analyze_detached_fname, R_OK) != 0)
  {
    fprintf(stderr, _("Song file %s not found\n"), analyze_detached_fname);
    exit(1);
  }
  if ( (data = create_daemon_data(gjay))==NULL)
	exit(1);
  data->mode = ANALYZE_DETACHED;
  analyze(data, analyze_detached_fname, TRUE);
  destroy_gjay_ipc(data->ipc);
}

void run_as_daemon(GjayApp *gjay, gjay_mode mode)
{
  gchar *path;
  FILE *fp;
  struct daemon_data *data;

  /* Write pid to ~/.gjay/gjay.pid */
  path = g_strdup_printf("%s/%s/%s", g_get_home_dir(),
      GJAY_DIR, GJAY_PID);
  if ((fp = fopen(path, "w")) == NULL) {
    g_error(_("Cannot open %s for writing\n"), path);
    exit(1);
  }
  g_free(path);
  fprintf(fp, "%d", getpid());
  fclose(fp);
    
  signal(SIGTERM, kill_signal);
  signal(SIGKILL, kill_signal);
  signal(SIGINT,  kill_signal);
    
  if ( (data = create_daemon_data(gjay))==NULL)
	exit(1);
  data->mode = mode;
  analysis_daemon(data);
  destroy_gjay_ipc(data->ipc);
}

/**
 * When the daemon receives a kill signal, delete ~/.gjay/gjay.pid
 */
static void kill_signal (int sig) {
  gchar *path;

  /* Write pid to ~/.gjay/gjay.pid */
  path = g_strdup_printf("%s/%s/%s", g_get_home_dir(),
      GJAY_DIR, GJAY_PID);
  unlink(path); 
  g_free(path);
  exit(0);
}

static void add_file_to_queue ( char * fname, const int verbosity) {
    char queue_fname[BUFFER_SIZE], buffer[BUFFER_SIZE], * str;
    FILE * f_queue, * f_add;
        
    if (verbosity > 1) {
        printf(_("Adding '%s' to analysis queue\n"), fname);
    }
    snprintf(queue_fname, BUFFER_SIZE, "%s/%s/%s", getenv("HOME"), 
             GJAY_DIR, GJAY_QUEUE);
    f_queue = fopen(queue_fname, "a");
    f_add = fopen(fname, "r");
    
    if (f_queue && f_add) {
        while (!feof(f_add)) {
            read_line(f_add, buffer, BUFFER_SIZE);
            if (!g_hash_table_lookup(queue_hash, buffer)) {
                str = g_strdup(buffer);
                queue = g_list_append(queue, str);
                g_hash_table_insert(queue_hash, str, (void *) 1);
                fprintf(f_queue, "%s\n", str);
            }
        }
        fclose(f_queue);
        fclose(f_add);
    }
}



static void write_queue (void) {
    char fname[BUFFER_SIZE], fname_temp[BUFFER_SIZE];
    FILE * f;
    GList * llist;
    
    snprintf(fname, BUFFER_SIZE, "%s/%s/%s", getenv("HOME"), 
             GJAY_DIR, GJAY_QUEUE);
    snprintf(fname_temp, BUFFER_SIZE, "%s_temp", fname);
    
    if (queue) {
        f = fopen(fname_temp, "w");
        if (f) {
            for (llist = g_list_first(queue);
                 llist;
                 llist = g_list_next(llist)) {
                fprintf(f, "%s\n", (char *) llist->data);
            }
            rename(fname_temp, fname);
            fclose(f);
        }
    } else {
        unlink(fname);
    }
}



gboolean ui_pipe_input (GIOChannel *source,
                        GIOCondition condition,
                        gpointer user_data) {
    GList * list;
    char buffer[BUFFER_SIZE], * file;
    int len, k, l;
    ipc_type ipc;
	struct daemon_data *ddata = (struct daemon_data*)user_data;

    if (read(ddata->ipc->ui_fifo, &len, sizeof(int)) < 1) {
        g_critical(_("Unable to read UI pipe"));
        return TRUE;
    }
    if (len >= BUFFER_SIZE) {
        g_critical(_("UI sent messae larger than buffer"));
        return TRUE;
    }
    for (k = 0, l = len; l; l -= k) {
        k = read(ddata->ipc->ui_fifo, buffer + k, l);
    }
    
    ddata->last_ping = time(NULL);
    
    memcpy((void *) &ipc, buffer, sizeof(ipc_type));
    switch(ipc) {
    case REQ_ACK:
        send_ipc(ddata->ipc->daemon_fifo, ACK);
        break;
    case ACK:
        if (ddata->verbosity > 1)
            printf(_("Daemon received ack\n"));
        // No need for action
        break;
    case UNLINK_DAEMON_FILE:
        snprintf(buffer, BUFFER_SIZE, "%s/%s/%s", 
                 getenv("HOME"), GJAY_DIR, GJAY_DAEMON_DATA);
        if (ddata->verbosity > 1)
            printf(_("Deleting daemon pid file '%s'\n"), buffer);
        unlink(buffer);
        break;
    case CLEAR_ANALYSIS_QUEUE:
        snprintf(buffer, BUFFER_SIZE, "%s/%s/%s", getenv("HOME"), 
                 GJAY_DIR, GJAY_QUEUE);
        if (ddata->verbosity > 1)
            printf(_("Daemon is clearing out analysis queue, deleting file '%s'\n"), buffer);
        unlink(buffer);
        for (list = g_list_first(queue); list; list = g_list_next(list)) 
            g_free(list->data);
        g_list_free(queue);
        queue = NULL;
        g_hash_table_destroy(queue_hash);
        queue_hash = g_hash_table_new(g_str_hash, g_str_equal);
        break;
    case QUEUE_FILE:
        /* If the file is not already in the queue, enqueue the file and
         * also append the file name to the analysis log */
        buffer[len] = '\0';
        file = buffer + sizeof(ipc_type);
        if (ddata->verbosity > 1) 
            printf(_("Daemon queuing song file '%s'\n"), file);
        add_file_to_queue(file, ddata->verbosity);
        g_idle_add (daemon_idle, ddata);
        unlink(file);
        break;
    case DETACH: 
        if (ddata->mode == DAEMON) {
            if (ddata->verbosity)
                printf(_("Detaching daemon\n"));
            ddata->mode = DAEMON_DETACHED;
            g_idle_add (daemon_idle, ddata);
        }
        break;
    case ATTACH:
        if (ddata->mode != DAEMON) {
            if (ddata->verbosity)
                printf(_("Attaching to daemon\n"));
            ddata->mode = DAEMON;
            if(ddata->in_analysis && ddata->analyze_song)
                send_analyze_song_name(ddata->ipc->daemon_fifo, ddata->analyze_song);
        }
        
        break;
    case QUIT_IF_ATTACHED:
        if (ddata->mode == DAEMON) {
            if (ddata->verbosity)
                printf(_("Daemon Quitting\n"));
            g_main_quit ((GMainLoop * ) ddata->loop);
        }
        break;
    default:
        // Do nothing
        break;
    }
    return TRUE;
}



static void
analyze(struct daemon_data *ddata,
	    const char * fname,            /* File to analyze */
        const gboolean result_to_stdout) /* Write result to stdout? */
{
    FILE * f;
    gchar * utf8;
    wav_file wsfile;
    int result, i;
    char buffer[BUFFER_SIZE];
    gdouble freq[NUM_FREQ_SAMPLES], volume_diff, analyze_bpm;
    gboolean is_song;
    song_file_type type;
    time_t t=0;

    ddata->analyze_song = NULL;
    ddata->in_analysis = TRUE;
    
    send_ui_percent(ddata->ipc->daemon_fifo, 0);
    if (fname == NULL || fname[0] == '\0') {
      ddata->in_analysis = FALSE;
      return;
    }
    
    if (access(fname, R_OK) != 0) {
        /* File ain't there! The UI thread will check for non-existant
           files periodically. */
        if (ddata->verbosity)
            g_warning(_("analyze(): File '%s' cannot be read\n"),fname);
        ddata->in_analysis = FALSE;
        return;
    }

    ddata->analyze_song = create_song();

    utf8 = strdup_to_utf8(fname);
    song_set_path(ddata->analyze_song, utf8);
    g_free(utf8);

    file_info(ddata->verbosity,
			  ddata->ogg_supported, ddata->flac_supported,
			  ddata->analyze_song->path, 
              &is_song, 
              &ddata->analyze_song->inode,
              &ddata->analyze_song->dev,
              &ddata->analyze_song->length,
              &ddata->analyze_song->title,
              &ddata->analyze_song->artist,
              &ddata->analyze_song->album, 
              &type);

    if (!is_song) {
        if (ddata->verbosity)
          g_warning(_("File '%s' is not a recognised song.\n"),fname);
        ddata->in_analysis = FALSE;
		delete_song(ddata->analyze_song);
        return;
    }
    
    send_analyze_song_name(ddata->ipc->daemon_fifo, ddata->analyze_song);
    send_ipc_text(ddata->ipc->daemon_fifo, ANIMATE_START, ddata->analyze_song->path);

    if ( (f = inflate_to_wav(ddata, fname, type)) == NULL)
    {
      if (ddata->verbosity)
        g_warning(_("Unable to inflate song '%s'.\n"), fname);
      ddata->in_analysis = FALSE;
	  delete_song(ddata->analyze_song);
      return;
    }
    memset(&wsfile, 0x00, sizeof(wav_file));
    wsfile.f = f;
    if (fread(&wsfile.header, sizeof(waveheaderstruct), 1, f) < 1) {
        g_warning(_("Unable to read WAV header '%s'.\n"), fname);
        ddata->in_analysis = FALSE;
        delete_song(ddata->analyze_song);
        return;
    }
	/* Check to see if the decoder really decoded */
	if (wsfile.header.main_chunk[0] == '\0') {
	  if (ddata->verbosity)
		g_warning(_("Decoding of song '%s' failed, bad wav header.\n"), fname);
	  ddata->in_analysis = FALSE;
	  delete_song(ddata->analyze_song);
	  return;
	}
    wav_header_swab(&wsfile.header);
    wsfile.header.data_length = (MAX(1, ddata->analyze_song->length - 1)) * wsfile.header.byte_p_sec;

    if (ddata->verbosity) {
        printf(_("Analyzing song file '%s'\n"), fname);
        t = time(NULL);
    }

    result = run_analysis(ddata, &wsfile, freq, &volume_diff, &analyze_bpm); 

    if (ddata->verbosity) 
        printf(_("Analysis took %ld seconds\n"), time(NULL) - t);
    
    send_ui_percent(ddata->ipc->daemon_fifo, 0);

    if (result && ddata->analyze_song) {
        for (i = 0; i < NUM_FREQ_SAMPLES; i++) 
            ddata->analyze_song->freq[i] = freq[i];
        ddata->analyze_song->bpm = analyze_bpm;
        if (isinf(ddata->analyze_song->bpm) || (ddata->analyze_song->bpm < MIN_BPM)) {
            ddata->analyze_song->bpm_undef = TRUE;
        } else {
            ddata->analyze_song->bpm_undef = FALSE;
        }
        ddata->analyze_song->volume_diff = volume_diff;
        ddata->analyze_song->no_data = FALSE;
    } 

    /* Finish reading rest of output */
    while ((result = fread(buffer, 1,  BUFFER_SIZE, f)))
        ;
    fclose(f);

    send_ipc(ddata->ipc->daemon_fifo, ANIMATE_STOP);
    send_ipc_text(ddata->ipc->daemon_fifo, STATUS_TEXT, "Idle");

    if (result_to_stdout) {
        write_song_data(stdout, ddata->analyze_song);
    } else {
        result = append_daemon_file(ddata->analyze_song);
    }
    if (result >= 0) 
        send_ipc_int(ddata->ipc->daemon_fifo, ADDED_FILE, result);
    delete_song(ddata->analyze_song);
    ddata->in_analysis = FALSE;
    ddata->analyze_song = NULL;
    return;
}

static void
check_decoders(struct daemon_data *ddata)
{
  if (ddata->ogg_decoder == NULL)
    ddata->ogg_decoder = g_find_program_in_path(OGG_DECODER_APP);

  if (ddata->mp3_decoder == NULL)
    ddata->mp3_decoder = g_find_program_in_path(MP3_DECODER_APP1);
  if (ddata->mp3_decoder == NULL)
    ddata->mp3_decoder = g_find_program_in_path(MP3_DECODER_APP2);

  if (ddata->flac_decoder == NULL)
    ddata->flac_decoder = g_find_program_in_path(FLAC_DECODER_APP);
}

static FILE *
inflate_to_wav (struct daemon_data *ddata, const gchar * path, const song_file_type type)
{
  gchar *cmdline;
  gchar *quoted_path;
  FILE *fp;

  quoted_path = g_shell_quote(path);
  if (ddata->mp3_decoder == NULL)
    check_decoders(ddata);

  switch (type) {
    case OGG:
      if (ddata->ogg_decoder == NULL) {
        if (ddata->verbosity)
          g_warning(_("Unable to decode '%s' as no ogg decoder found"), path);
        return NULL;
      }
      cmdline = g_strdup_printf("%s %s -d wav -f - 2> /dev/null",
          ddata->ogg_decoder,
          quoted_path);
      break;
    case MP3:
      if (ddata->mp3_decoder == NULL) {
        if (ddata->verbosity)
          g_warning(_("Unable to decode '%s' as no mp3 decoder found"), path);
        return NULL;
      }
      cmdline = g_strdup_printf("%s -w - -b 10000 %s 2> /dev/null",
          ddata->mp3_decoder,
          quoted_path);
      break;
    case FLAC:
      if (ddata->flac_decoder == NULL) {
        if (ddata->verbosity)
          g_warning(_("Unable to decode '%s' as no flac decoder found"), path);
        return NULL;
      }
      cmdline = g_strdup_printf("%s -d -c %s 2> /dev/null",
          ddata->flac_decoder,
          quoted_path);
      break;
    case WAV:
    default:
      g_free(quoted_path);
      return fopen(path, "r");
  }
  g_free(quoted_path);
  //fprintf(stderr,"Decoding with:\n ->%s\n->%s", cmdline,path);
  if (!(fp = popen(cmdline, "r"))) {
    g_error(_("Unable to run decoder command '%s'\n"), cmdline);
    g_free(cmdline);
      return NULL;
  } 
  g_free(cmdline);
  return fp;
}


/* Swap the byte order of wav header. Wavs are stored little-endian, so this
   is necessary when using on big-endian (e.g. PPC) machines */
void
wav_header_swab(waveheaderstruct * header)
{
    header->length = le32_to_cpu(header->length);
    header->length_chunk = le32_to_cpu(header->length_chunk);
    header->data_length = le32_to_cpu(header->data_length);
    header->sample_fq = le32_to_cpu(header->sample_fq);
    header->byte_p_sec = le32_to_cpu(header->byte_p_sec);
    header->format = le16_to_cpu(header->format);
    header->modus = le16_to_cpu(header->modus);
    header->byte_p_spl = le16_to_cpu(header->byte_p_spl);
    header->bit_p_spl = le16_to_cpu(header->bit_p_spl);
}

/**
 * This is the big, bad analysis algorithm. To make things REALLY complicated,
 * I'm interspersing two algorithsm -- BPM and frequency analysis -- such that
 * we only have to do one decode pass. Whee!
 *
 * BPM algorithm taken from BpmDJ by Werner Van Belle.
 *
 * Frequency algorithm taken from spectromatic by Daniel Franklin.
 *
 * Any ugliness is the fault of my munging. 
 */
static int
run_analysis  (struct daemon_data *ddata,
				   wav_file * wsfile,
                   gdouble * freq_results,
                   gdouble * volume_diff,
                   gdouble * bpm_result )
{
    int16_t *freq_data;
    double *ch1 = NULL, *ch2 = NULL, *mags = NULL;
    long i, j, k, bin;
    double *total_mags;
    double sum, frame_sum, max_frame_sum, g_factor, freq, g_freq;
    signed short buffer[BPM_BUF_SIZE];
    long count, pos, h, redux, num_frames;
    unsigned long startshift = 0, stopshift = 0;
    unsigned char *audio;
    unsigned long audiosize;

    if (wsfile->header.modus != 1 && wsfile->header.modus != 2) {
      if (ddata->verbosity > 2)
        g_warning(_("File is not a (converted) wav file. Modus is supposed to be 1 or 2 but is %d\n"), wsfile->header.modus);
        // Not a wav file
        return FALSE;
    }
    
    if (wsfile->header.byte_p_spl / wsfile->header.modus != 2) {
      if (ddata->verbosity > 2)
        g_warning(_("File is not a 16-bit stream\n"));
        // Not 16-bit
        return FALSE;
    }

    /* Spectrum set-up */    
    read_buffer_size = WINDOW_SIZE*4;
    read_buffer = malloc(read_buffer_size);
    read_buffer_start = 0;
    read_buffer_end = read_buffer_size;
    freq_data = (int16_t*) g_malloc0 (WINDOW_SIZE * wsfile->header.byte_p_spl);
    mags = (double*) g_malloc0 (WINDOW_SIZE / 2 * sizeof (double));
    total_mags = (double*) g_malloc0 (WINDOW_SIZE / 2 * sizeof (double));
    //memset (total_mags, 0x00, WINDOW_SIZE / 2 * sizeof (double));
    memset (freq_results, 0x00, NUM_FREQ_SAMPLES * sizeof(double));
    ch1 = (double*) g_malloc0 (WINDOW_SIZE * sizeof (double));
    if (wsfile->header.modus == 2)
        ch2 = (double*) g_malloc0 (WINDOW_SIZE * sizeof (double));
    num_frames = 0;
    max_frame_sum = 0;
    sum = 0;

    /* BPM set-up */
    audiosize = wsfile->header.data_length;
    audiosize/=(4*(44100/AUDIO_RATE));
    audio= g_malloc0(audiosize+1);
    assert(audio);
    pos=0;

    /* Read the first chunk of the file into the shared buffer */
    if (fread(wsfile->buffer, 1, SHARED_BUF_SIZE, wsfile->f) < 1) {
      if (ddata->verbosity > 2)
        g_warning(_("Cannot load WAV file first chunk"));
        return FALSE;
    }
    wsfile->seek = SHARED_BUF_SIZE;

    /* Copy the first bit of this into the freq. analysis buffer */
    memcpy(read_buffer, wsfile->buffer, read_buffer_size);
    wsfile->freq_seek = read_buffer_size;

    /* In this main loop, we read the entire file. Most of this loop 
     * represents the spectrum algorithm; the marked tight loop is the bpm
     * algorithm. */
    for (i = -WINDOW_SIZE; i < WINDOW_SIZE + (int)(wsfile->header.data_length / wsfile->header.byte_p_spl); i += STEP_SIZE) {

        freq_read_frames (wsfile, i, WINDOW_SIZE, (char *)freq_data);

        if (wsfile->freq_seek == SHARED_BUF_SIZE) {
            /* At end of buffer. Read some more data. */
            count = fread(wsfile->buffer, 1, SHARED_BUF_SIZE, wsfile->f);
            memcpy(buffer, wsfile->buffer, SHARED_BUF_SIZE);
            wsfile->freq_seek = 0;
            wsfile->seek += count;
            
            /* Update status bar. This chunk takes ~70% of the time  */
            send_ui_percent(ddata->ipc->daemon_fifo, wsfile->seek/(wsfile->header.data_length / 70));
            
            /* BPM loop */
            for (h=0;h<count/2;h+=2*(44100/AUDIO_RATE))
            {
                signed long int left, right,mean;
                left=abs(buffer[h]);
                right=abs(buffer[h+1]);
                mean=(left+right)/2;
                redux=abs(mean)/128;
                if (pos+h/(2*(44100/AUDIO_RATE))>=audiosize) break;
                assert(pos+h/(2*(44100/AUDIO_RATE))<audiosize);
                audio[pos+h/(2*(44100/AUDIO_RATE))]=(unsigned char)redux;
            }
            pos+=count/(4*(44100/AUDIO_RATE));
        }
        
        /* The rest of this loop is spectrum analysis */
        if (wsfile->header.modus == 1) {
            for (j = 0; j < WINDOW_SIZE; j++)
                ch1 [j] = (int16_t)le16_to_cpu(freq_data[j]);
            gsl_fft_real_radix2_transform (ch1, 1, WINDOW_SIZE);
            
        } else {
            for (j = 0, k = 0; k < WINDOW_SIZE; k++, j+=2) {
                ch1[k] = (double)((int16_t)le16_to_cpu(freq_data[j]));
                ch2[k] = (double)((int16_t)le16_to_cpu(freq_data[j+1]));
            } 
            gsl_fft_real_radix2_transform (ch1, 1, WINDOW_SIZE);
            gsl_fft_real_radix2_transform (ch2, 1, WINDOW_SIZE);
        }
        
//        mags [0] = fabs (ch1 [0]);
        
        for (j = 0; j < WINDOW_SIZE / 2; j++) {
	    double square_this = ch1 [j] * ch1 [j] + ch1 [WINDOW_SIZE - j] * ch1 [WINDOW_SIZE - j];
	    if (square_this > 0.0 && square_this < BROKEN_VAL)
        	mags [j] = sqrt (square_this);
    	    else
    		mags [j] = 0;
        }

        /* Add magnitudes */
        for (frame_sum = 0, k = 0; k < WINDOW_SIZE / 2; k++) {
            total_mags[k] += mags[k];
            frame_sum += mags[k];
        }

        max_frame_sum = MAX(frame_sum, max_frame_sum);
        sum += frame_sum;
        num_frames++;

        if (wsfile->header.modus == 2) {
            for (j = 0; j < WINDOW_SIZE / 2; j++) {
		double square_this = ch2 [j] * ch2 [j] + ch2 [WINDOW_SIZE - j] * ch2 [WINDOW_SIZE - j];
		if ( square_this > 0 && square_this < BROKEN_VAL)
        	    mags [j] = sqrt (square_this);
    		else
    		    mags [j] = 0;
            }

            /* Add magnitudes */            
            for (frame_sum = 0, k = 0; k < WINDOW_SIZE / 2; k++) {
                total_mags[k] += mags[k];
                frame_sum += mags[k];
            }
        
            max_frame_sum = MAX(frame_sum, max_frame_sum);
            sum += frame_sum;
            num_frames++;
        }
    }

    /* Finish analysis... */
    *volume_diff =  max_frame_sum / (sum / (gdouble) num_frames);

    for (k = 0; k < WINDOW_SIZE / 2; k++) 
        total_mags[k] = (total_mags[k] / sum) * MAGS_MULTIPLIER;
    
    g_freq = START_FREQ;
    g_factor = exp( log(MAX_FREQ/START_FREQ) / NUM_FREQ_SAMPLES);
    bin = 0;
    for (k = 0; k < WINDOW_SIZE / 2; k++) {
        /* Determine which frequency band this sample falls into */
        freq = (k * MAX_FREQ ) / (WINDOW_SIZE / 2);
        if (freq > g_freq) {
            bin = MIN(bin + 1, NUM_FREQ_SAMPLES - 1);
            g_freq *= g_factor;
        }
        freq_results[bin] += total_mags[k];
    }

    if (ddata->verbosity > 1) {
        printf(_("Frequencies: \n"));
        for (k = 0; k < NUM_FREQ_SAMPLES; k++) 
            printf("%f ", freq_results[k]);
        printf("\n");
    }

    /* Complete BPM analysis */
    stopshift=AUDIO_RATE*60*4/START_BPM;
    startshift=AUDIO_RATE*60*4/STOP_BPM;
    {
	unsigned long foutat[stopshift-startshift];
	unsigned long fout, minimumfout=0, maximumfout,minimumfoutat=ULONG_MAX,
                left,right;
        memset(&foutat,0,sizeof(foutat));
	for(h=startshift;h<stopshift;h+=50)
        {
            fout=bpm_phasefit(h, audio, audiosize);
            foutat[h-startshift]=fout;
            if (minimumfout==0) maximumfout=minimumfout=fout;
            if (fout<minimumfout) 
            {
                minimumfout=fout;
                minimumfoutat=h;
            }
            if (fout>maximumfout) maximumfout=fout;
            
            send_ui_percent (ddata->ipc->daemon_fifo, 70 + ((15*(h - startshift)) / (stopshift - startshift)));
        }
        left=minimumfoutat-100;
	right=minimumfoutat+100;
	if ( left < startshift ) 
            left = startshift;
	if ( right > stopshift ) 
            right = stopshift;
	for(h=left; h<right; h++) {
            fout=bpm_phasefit(h, audio, audiosize);
            foutat[h-startshift]=fout;
            if (minimumfout==0) maximumfout=minimumfout=fout;
            if (fout<minimumfout) 
            {
                minimumfout=fout;
                minimumfoutat=h;
            }
            if (fout>maximumfout) maximumfout=fout;
            
            send_ui_percent(ddata->ipc->daemon_fifo, 85 + ((15*(h - left)) / (right - left)));
        }
        
        for(h=startshift;h<stopshift;h++) {
            fout=foutat[h-startshift];
            if (fout)
            {
                fout-=minimumfout;
            }
        }
        *bpm_result = 4.0*(double)AUDIO_RATE*60.0/(double)minimumfoutat;
        if (ddata->verbosity > 1) 
            printf(_("BPM: %f\n"), *bpm_result);
    }
    g_free (audio);
    g_free (freq_data);
    g_free (mags);
    g_free (total_mags);
    g_free (read_buffer);
    return TRUE;
}


static int
freq_read_frames (wav_file * wsfile, int start, int length, void *data)
{
    int realstart = start;
    int reallength = length;
    int offset = 0;
    int seek, len;
    
    if (start + length < 0 || start > (int)(wsfile->header.data_length / wsfile->header.byte_p_spl)) {
        memset (data, 0, length * wsfile->header.byte_p_spl);
        return 0;
    }
    
    if (start < 0) {
        offset = -start;
        memset (data, 0, offset * wsfile->header.byte_p_spl);
        realstart = 0;
        reallength += start;
    }
    
    if (start + length > (int)(wsfile->header.data_length / wsfile->header.byte_p_spl)) {
        reallength -= start + length - (wsfile->header.data_length / wsfile->header.byte_p_spl);
        memset (data, 0, reallength * wsfile->header.byte_p_spl);
    }
    
    seek = ((realstart + offset) * wsfile->header.byte_p_spl);
    len = wsfile->header.byte_p_spl * reallength;
    if (seek + len <= read_buffer_size) {
        memcpy(data + offset * wsfile->header.byte_p_spl,
               read_buffer + seek,
               wsfile->header.byte_p_spl * reallength);
    } else {
        if (seek + len > read_buffer_end) {
            char * new_buffer = malloc(read_buffer_size);
            int shift = seek + len - read_buffer_end;
            memcpy(new_buffer, 
                   read_buffer + shift,
                   read_buffer_size - shift);
            free(read_buffer);
            read_buffer = new_buffer;

            memcpy (read_buffer + read_buffer_size - shift,
                    wsfile->buffer + wsfile->freq_seek,
                    shift);
            wsfile->freq_seek += shift;

            read_buffer_start += shift;
            read_buffer_end += shift;
        }
        memcpy(data + offset * wsfile->header.byte_p_spl,
               read_buffer + seek - read_buffer_start,
               wsfile->header.byte_p_spl * reallength);
    }
    return 1;
}


static unsigned long
bpm_phasefit(const long i, const unsigned char const *audio,
    const unsigned long audiosize)
{
   long c,d;
   unsigned long mismatch=0;
   unsigned long prev=mismatch;
   for(c=i;c<audiosize;c++)
     {
	d=abs((long)audio[c]-(long)audio[c-i]);
	prev=mismatch;
	mismatch+=d;
	assert(mismatch>=prev);
     }
   return mismatch;
}



static void
send_ui_percent (const int fd, int percent)
{
  static int old_percent = -1;

  if (old_percent == percent)
    return;
    
  if (percent < 0)
	percent = 0;
  if (percent > 100)
	percent = 100;
  old_percent = percent;
    
  send_ipc_int(fd, STATUS_PERCENT, percent);

  /* This is a bit of a hack, but... anytime we care enough to pause
   * analysis so we can send the percentage,  run an iteration of the 
   * event loop */
  if (g_main_pending())
    g_main_iteration(TRUE);
}


static void
send_analyze_song_name ( const int fd, GjaySong *song )
{
  char buffer[BUFFER_SIZE];
  if (!song)
    return;
  if (song->title && song->artist)
	g_snprintf(buffer, BUFFER_SIZE, "%s : %s", song->artist, song->title);
  else
	g_snprintf(buffer, BUFFER_SIZE, "%s", song->path);
  strncpy(buffer + 60, "...\0", 4);
  send_ipc_text(fd, STATUS_TEXT, buffer);
}



static void
analysis_daemon(struct daemon_data *ddata) {
    GIOChannel * ui_io;
    char buffer[BUFFER_SIZE];
    gchar * file;
    FILE * f;
    
    /* Nice the analysis */
    setpriority(PRIO_PROCESS, getpid(), 19);
    
    ddata->in_analysis = FALSE;
    ddata->loop = g_main_new(FALSE);
    ddata->last_ping = time(NULL);
    queue_hash = g_hash_table_new(g_str_hash, g_str_equal);

    /* Read analysis queue, if any */
    snprintf(buffer, BUFFER_SIZE, "%s/%s/%s", getenv("HOME"), 
             GJAY_DIR, GJAY_QUEUE);
    f = fopen(buffer, "r");
    if (f) {
        while (!feof(f)) {
            read_line(f, buffer, BUFFER_SIZE);
            if (strlen(buffer) &&!g_hash_table_lookup(queue_hash, buffer)) {
                file = g_strdup(buffer);
                g_hash_table_insert(queue_hash, file, (void *) 1);
                queue = g_list_append(queue, file);
            }
        }
        fclose(f);
    }
    ui_io = g_io_channel_unix_new (ddata->ipc->ui_fifo);
    g_io_add_watch (ui_io,
                    G_IO_IN,
                    ui_pipe_input,
                    ddata);
    g_idle_add (daemon_idle, ddata);
    // FIXME: add G_IO_HUP watcher

    g_main_run(ddata->loop);
}


gboolean daemon_idle (gpointer data) {
    gchar * file;
	struct daemon_data *ddata = (struct daemon_data*)data;

    if ((ddata->mode != DAEMON_DETACHED) && 
        (time(NULL) - ddata->last_ping > DAEMON_ATTACH_FREAKOUT)) {
        if (ddata->verbosity)
            printf(_("Daemon appears to have been orphaned. Quitting.\n"));
        g_main_quit(ddata->loop);
    } 

    if (ddata->mode == DAEMON_INIT) {
        usleep(SLEEP_WHILE_IDLE);
        return TRUE;
    }

    if (ddata->in_analysis)
        return TRUE;
    
    if (queue) {
        file = g_list_first(queue)->data;
        analyze(ddata, file, FALSE);
        g_hash_table_remove(queue_hash, file);
        queue = g_list_remove(queue, file);
        g_free(file);
        
        write_queue();
    }
    
    if (queue)
        return TRUE;

    if (ddata->mode == DAEMON_DETACHED) {
        if (ddata->verbosity)
            printf(_("Analysis daemon done.\n"));
        //g_main_quit((GMainLoop *) data);
    }
    return FALSE;
}

static struct daemon_data *create_daemon_data(GjayApp *gjay){
  struct daemon_data *ddata;

  if ( (ddata = g_malloc0(sizeof(struct daemon_data))) == NULL)
	return NULL;

  /*Copy the items */
  ddata->verbosity = gjay->verbosity;
  ddata->ogg_supported = gjay->ogg_supported;
  ddata->flac_supported = gjay->flac_supported;
  create_gjay_ipc(&(ddata->ipc));

  ddata->in_analysis = FALSE;
  ddata->analyze_song = NULL;

  ddata->mp3_decoder = NULL;
  ddata->ogg_decoder = NULL;
  ddata->flac_decoder = NULL;
  return ddata;
}
