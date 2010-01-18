/**
 * GJay, copyright (c) 2002 Chuck Groom. Sections of this code come
 * from spectromatic, copyright (C) 1997-2002 Daniel Franklin, and 
 * BpmDJ by Werner Van Belle.
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
 * Analysis.c -- manages the background threads and processes involved
 * in song analysis.
 */

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

#define BPM_BUF_SIZE    32*1024
#define SHARED_BUF_SIZE sizeof(short int) * BPM_BUF_SIZE
#define SLEEP_WHILE_IDLE 500


typedef struct {
    FILE * f;
    waveheaderstruct header;
    long int freq_seek, seek;
    char buffer[SHARED_BUF_SIZE];
} wav_file;



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
static gboolean     in_analysis = FALSE;  /* Are we currently analyizing a
                                         song? */
static song       * analyze_song = NULL;
static GList      * queue = NULL; 
static GHashTable * queue_hash = NULL;
static time_t       last_ping;

static FILE *        inflate_to_wav (gchar * path, 
                              song_file_type type);
static int           run_analysis     ( wav_file * wsfile,
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
static void          send_ui_percent        ( int percent );
static void          send_analyze_song_name ( void );
static void   write_queue       ( void );
static void kill_signal (int sig);
static int      quote_path(char *buf, size_t bufsiz, const char *path);
static void analysis_daemon(void);
gboolean      daemon_idle       ( gpointer data );

void gjay_init_daemon(void)
{
  gchar *path;
  FILE *fp;

  /* Write pid to ~/.gjay/gjay.pid */
  path = g_strdup_printf("%s/%s/%s", g_get_home_dir(),
      GJAY_DIR, GJAY_PID);
  if ((fp = fopen(path, "w")) == NULL) {
    g_critical("Cannot open %s for writing\n", path);
    exit(1);
  }
  g_free(path);
  fprintf(fp, "%d", getpid());
  fclose(fp);
    
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
static void add_file_to_queue ( char * fname) {
    char queue_fname[BUFFER_SIZE], buffer[BUFFER_SIZE], * str;
    FILE * f_queue, * f_add;
        
    if (verbosity > 1) {
        printf("Adding '%s' to analysis queue\n", fname);
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
                        gpointer data) {
    GList * list;
    char buffer[BUFFER_SIZE], * file;
    int len, k, l;
    ipc_type ipc;

    read(ui_pipe_fd, &len, sizeof(int));
    assert(len < BUFFER_SIZE);
    for (k = 0, l = len; l; l -= k) {
        k = read(ui_pipe_fd, buffer + k, l);
    }
    
    last_ping = time(NULL);
    
    memcpy((void *) &ipc, buffer, sizeof(ipc_type));
    switch(ipc) {
    case REQ_ACK:
        send_ipc(daemon_pipe_fd, ACK);
        break;
    case ACK:
        if (verbosity > 1)
            printf("Daemon received ack\n");
        // No need for action
        break;
    case UNLINK_DAEMON_FILE:
        if (verbosity > 1)
            printf("Deleting daemon pid file\n");
        snprintf(buffer, BUFFER_SIZE, "%s/%s/%s", 
                 getenv("HOME"), GJAY_DIR, GJAY_DAEMON_DATA);
        unlink(buffer);
        break;
    case CLEAR_ANALYSIS_QUEUE:
        if (verbosity > 1)
            printf("Daemon is clearing out analysis queue, file\n");
        snprintf(buffer, BUFFER_SIZE, "%s/%s/%s", getenv("HOME"), 
                 GJAY_DIR, GJAY_QUEUE);
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
        if (verbosity > 1) 
            printf("Queuing file %s\n", file);
        add_file_to_queue(file);
        g_idle_add (daemon_idle, data);
        unlink(file);
        break;
    case DETACH: 
        if (mode == DAEMON) {
            if (verbosity)
                printf("Detaching daemon\n");
            mode = DAEMON_DETACHED;
            g_idle_add (daemon_idle, (GMainLoop * ) data);
        }
        break;
    case ATTACH:
        if (mode != DAEMON) {
            if (verbosity)
                printf("Attaching to daemon\n");
            mode = DAEMON;
            if(in_analysis && analyze_song)
                send_analyze_song_name();
        }
        
        break;
    case QUIT_IF_ATTACHED:
        if (mode == DAEMON) {
            if (verbosity)
                printf("Daemon Quitting\n");
            g_main_quit ((GMainLoop * ) data);
        }
        break;
    default:
        // Do nothing
        break;
    }
    return TRUE;
}



void analyze(char * fname,            /* File to analyze */
             gboolean result_to_stdout) /* Write result to stdout? 
                                           (Default FALSE means to disk */ 
{
    FILE * f;
    gchar * utf8;
    wav_file wsfile;
    int result, i;
    char buffer[BUFFER_SIZE];
    gdouble freq[NUM_FREQ_SAMPLES], volume_diff, analyze_bpm;
    gboolean is_song;
    song_file_type type;
    time_t t;

    analyze_song = NULL;
    in_analysis = TRUE;
    
    send_ui_percent(0);
    
    if (access(fname, R_OK) != 0) {
        /* File ain't there! The UI thread will check for non-existant
           files periodically. */
        if (verbosity)
            fprintf(stderr, "File not found\n");
        in_analysis = FALSE;
        return;
    }

    analyze_song = create_song();

    utf8 = strdup_to_utf8(fname);
    song_set_path(analyze_song, utf8);
    g_free(utf8);

    file_info(analyze_song->path, 
              &is_song, 
              &analyze_song->inode,
              &analyze_song->dev,
              &analyze_song->length,
              &analyze_song->title,
              &analyze_song->artist,
              &analyze_song->album, 
              &type);

    if (!is_song) {
        if (verbosity)
            fprintf(stderr, "File not song\n");
        in_analysis = FALSE;
        return;
    }
    
    send_analyze_song_name();
    send_ipc_text(daemon_pipe_fd, ANIMATE_START, analyze_song->path);

    f = inflate_to_wav(fname, type);
    memset(&wsfile, 0x00, sizeof(wav_file));
    wsfile.f = f;
    fread(&wsfile.header, sizeof(waveheaderstruct), 1, f);
    wav_header_swab(&wsfile.header);
    wsfile.header.data_length = (MAX(1, analyze_song->length - 1)) * wsfile.header.byte_p_sec;

    if (verbosity) {
        printf("Analyzing %s\n", fname);
        t = time(NULL);
    }

    result = run_analysis(&wsfile, freq, &volume_diff, &analyze_bpm); 

    if (verbosity) 
        printf("Analysis took %ld seconds\n", time(NULL) - t);
    
    send_ui_percent(0);

    if (result && analyze_song) {
        for (i = 0; i < NUM_FREQ_SAMPLES; i++) 
            analyze_song->freq[i] = freq[i];
        analyze_song->bpm = analyze_bpm;
        if (isinf(analyze_song->bpm) || (analyze_song->bpm < MIN_BPM)) {
            analyze_song->bpm_undef = TRUE;
        } else {
            analyze_song->bpm_undef = FALSE;
        }
        analyze_song->volume_diff = volume_diff;
        analyze_song->no_data = FALSE;
    } 

    /* Finish reading rest of output */
    while ((result = fread(buffer, 1,  BUFFER_SIZE, f)))
        ;
    fclose(f);

    send_ipc(daemon_pipe_fd, ANIMATE_STOP);
    send_ipc_text(daemon_pipe_fd, STATUS_TEXT, "Idle");

    if (result_to_stdout) {
        write_song_data(stdout, analyze_song);
    } else {
        result = append_daemon_file(analyze_song);
    }
    if (result >= 0) 
        send_ipc_int(daemon_pipe_fd, ADDED_FILE, result);
    delete_song(analyze_song);
    in_analysis = FALSE;
    analyze_song = NULL;
    return;
}

static FILE *
inflate_to_wav ( gchar * path, song_file_type type)
{
    char buffer[BUFFER_SIZE];
    char quoted_path[BUFFER_SIZE];
    FILE * f;

    quote_path(quoted_path, BUFFER_SIZE, path);
    switch (type) {
    case OGG:
        snprintf(buffer, BUFFER_SIZE, "%s \'%s\' -d wav -f - 2> /dev/null",
                 OGG_DECODER_APP,
                 quoted_path);
        break;
    case MP3:
        snprintf(buffer, BUFFER_SIZE, "%s -b 10000 \'%s\' -w - 2> /dev/null",
                 gjay->mp3_decoder_app,
                 quoted_path);
        break;
    case WAV:
    default:
        return fopen(path, "r");
    }
    if (!(f = popen(buffer, "r"))) {
        fprintf(stderr, "Unable to run %s\n", buffer);
        return NULL;
    } 
    return f;
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



/* Escape ' char in path */
static int
quote_path(char *buf, size_t bufsiz, const char *path)
{
    int in, out = 0;
    const char *quote = "\'\\\'\'"; /* a quoted quote character */
    for (in = 0; out < bufsiz && path[in] != '\0'; in++) {
        if (path[in] == '\'') {
            if (out + strlen(quote) > bufsiz) {
                break;
            } else {
                memcpy(&buf[out], quote, strlen(quote)); 
                out += strlen(quote);
            }
        } else {
            buf[out++] = path[in];
        }
    }
    if (out < bufsiz)
        buf[out++] = '\0';
    return out;
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
run_analysis  (wav_file * wsfile,
                   gdouble * freq_results,
                   gdouble * volume_diff,
                   gdouble * bpm_result )
{
    int16_t *freq_data;
    double *ch1 = NULL, *ch2 = NULL, *mags = NULL;
    long i, j, k, bin;
    double ch1max = 0, ch2max = 0;
    double *total_mags;
    double sum, frame_sum, max_frame_sum, g_factor, freq, g_freq;
    signed short buffer[BPM_BUF_SIZE];
    long count, pos, h, redux, startpos, num_frames;
    unsigned long startshift = 0, stopshift = 0;
    unsigned char *audio;
    unsigned long audiosize;

    if (wsfile->header.modus != 1 && wsfile->header.modus != 2) {
        // Not a wav file
        return FALSE;
    }
    
    if (wsfile->header.byte_p_spl / wsfile->header.modus != 2) {
        // Not 16-bit
        return FALSE;
    }

    /* Spectrum set-up */    
    read_buffer_size = WINDOW_SIZE*4;
    read_buffer = malloc(read_buffer_size);
    read_buffer_start = 0;
    read_buffer_end = read_buffer_size;
    freq_data = (int16_t*) malloc (WINDOW_SIZE * wsfile->header.byte_p_spl);
    mags = (double*) malloc (WINDOW_SIZE / 2 * sizeof (double));
    total_mags = (double*) malloc (WINDOW_SIZE / 2 * sizeof (double));
    memset (total_mags, 0x00, WINDOW_SIZE / 2 * sizeof (double));
    memset (freq_results, 0x00, NUM_FREQ_SAMPLES * sizeof(double));
    ch1 = (double*) malloc (WINDOW_SIZE * sizeof (double));
    if (wsfile->header.modus == 2)
        ch2 = (double*) malloc (WINDOW_SIZE * sizeof (double));
    num_frames = 0;
    max_frame_sum = 0;
    sum = 0;

    /* BPM set-up */
    audiosize = wsfile->header.data_length;
    audiosize/=(4*(44100/AUDIO_RATE));
    audio=malloc(audiosize+1);
    assert(audio);
    pos=0;
    startpos = pos;


    /* Read the first chunk of the file into the shared buffer */
    fread(wsfile->buffer, 1, SHARED_BUF_SIZE, wsfile->f);
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
            send_ui_percent(wsfile->seek/(wsfile->header.data_length / 70));
            
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
            
        } else {
            for (j = 0, k = 0; k < WINDOW_SIZE; k++, j+=2) {
                ch1[k] = (double)((int16_t)le16_to_cpu(freq_data[j]));
                ch2[k] = (double)((int16_t)le16_to_cpu(freq_data[j+1]));
            } 
            gsl_fft_real_radix2_transform (ch2, 1, WINDOW_SIZE);
        }
        gsl_fft_real_radix2_transform (ch1, 1, WINDOW_SIZE);
        
        mags [0] = fabs (ch1 [0]);
        
        for (j = 0; j < WINDOW_SIZE / 2; j++) {
            mags [j] = sqrt (ch1 [j] * ch1 [j] + ch1 [WINDOW_SIZE - j] * ch1 [WINDOW_SIZE - j]);
            
            if (mags [j] > ch1max)
                ch1max = mags [j];
        }
        
        /* Add magnitudes */
        for (k = 0; k < WINDOW_SIZE / 2; k++) 
            total_mags[k] += mags[k];
        
        if (wsfile->header.modus == 2) {
            mags [0] = fabs (ch2 [0]);
            
            for (j = 0; j < WINDOW_SIZE / 2; j++) {
                mags [j] = sqrt (ch2 [j] * ch2 [j] + ch2 [WINDOW_SIZE - j] * ch2 [WINDOW_SIZE - j]);
                
                if (mags [j] > ch2max)
                    ch2max = mags [j];
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
        total_mags[k] = total_mags[k] / sum;
    
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

    if (verbosity > 1) {
        printf("Frequencies: \n");
        for (k = 0; k < NUM_FREQ_SAMPLES; k++) 
            printf("%f ", freq_results[k]);
        printf("\n");
    }

    /* Complete BPM analysis */
    stopshift=AUDIO_RATE*60*4/START_BPM;
    startshift=AUDIO_RATE*60*4/STOP_BPM;
    {
	unsigned long foutat[stopshift-startshift];
	unsigned long fout, minimumfout=0, maximumfout,minimumfoutat,left,right;
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
            
            send_ui_percent (70 + ((15*(h - startshift)) / (stopshift - startshift)));
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
            
            send_ui_percent(85 + ((15*(h - left)) / (right - left)));
        }
        
        for(h=startshift;h<stopshift;h++) {
            fout=foutat[h-startshift];
            if (fout)
            {
                fout-=minimumfout;
            }
        }
        *bpm_result = 4.0*(double)AUDIO_RATE*60.0/(double)minimumfoutat;
        if (verbosity > 1) 
            printf("BPM: %f\n", *bpm_result);
    }
    free (audio);
    free (freq_data);
    free (mags);
    free (total_mags);
    free (read_buffer);
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
send_ui_percent (int percent)
{
    static int old_percent = -1;

    if (old_percent == percent)
        return;
    
    old_percent = percent;
    assert((percent >= 0) && (percent <= 100));
    
    send_ipc_int(daemon_pipe_fd, STATUS_PERCENT, percent);

    /* This is a bit of a hack, but... anytime we care enough to pause
     * analysis so we can send the percentage,  run an iteration of the 
     * event loop */
    if (g_main_pending())
        g_main_iteration(TRUE);
}


static void
send_analyze_song_name ( void )
{
    char buffer[BUFFER_SIZE];
    if (!analyze_song)
        return;
    if (analyze_song->title && analyze_song->artist) {
        snprintf(buffer, BUFFER_SIZE, "%s : %s", 
                 analyze_song->artist, 
                 analyze_song->title);
    } else {
        snprintf(buffer, BUFFER_SIZE, "%s", analyze_song->path);
    }
    strncpy(buffer + 60, "...\0", 4);
    send_ipc_text(daemon_pipe_fd, STATUS_TEXT, buffer);
}



static void
analysis_daemon(void) {
    GIOChannel * ui_io;
    GMainLoop * loop;
    char buffer[BUFFER_SIZE];
    gchar * file;
    FILE * f;
    
    /* Nice the analysis */
    setpriority(PRIO_PROCESS, getpid(), 19);
    
    in_analysis = FALSE;
    loop = g_main_new(FALSE);
    last_ping = time(NULL);
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
    ui_io = g_io_channel_unix_new (ui_pipe_fd);
    g_io_add_watch (ui_io,
                    G_IO_IN,
                    ui_pipe_input,
                    loop);
    g_idle_add (daemon_idle, loop);
    // FIXME: add G_IO_HUP watcher

    g_main_run(loop);
}


gboolean daemon_idle (gpointer data) {
    gchar * file;

    if ((mode != DAEMON_DETACHED) && 
        (time(NULL) - last_ping > DAEMON_ATTACH_FREAKOUT)) {
        if (verbosity)
            printf("Daemon appears to have been orphaned. Quitting.\n");
        g_main_quit((GMainLoop *) data);
    } 

    if (mode == DAEMON_INIT) {
        usleep(SLEEP_WHILE_IDLE);
        return TRUE;
    }

    if (in_analysis)
        return TRUE;
    
    if (queue) {
        file = g_list_first(queue)->data;
        analyze(file, FALSE);
        g_hash_table_remove(queue_hash, file);
        queue = g_list_remove(queue, file);
        g_free(file);
        
        write_queue();
    }
    
    if (queue)
        return TRUE;

    if (mode == DAEMON_DETACHED) {
        if (verbosity)
            printf("Analysis daemon done.\n");
        g_main_quit((GMainLoop *) data);
    }
    return FALSE;
}

