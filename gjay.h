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
 */

#ifndef GJAY_H
#define GJAY_H

#include <stdio.h>
#include <gtk/gtk.h>
#include <glib.h>
#include <assert.h>
#include <limits.h>
#include "rgbhsv.h"
#include "songs.h"
#include "prefs.h"
#include "ui.h"


/* Default directory for storing app info */
#define GJAY_VERSION   "0.2"
#define GJAY_DIR       ".gjay"
#define GJAY_PREFS     "prefs"
#define GJAY_QUEUE     "analysis_queue"
/* Three files define GJay's information about songs. 
 *   FILE_DATA -- Store permanent information about song characteristics.
 *                These are features which never change (BPM, Freq, etc.)
 *   DAEMON_DATA -- Store transient information about song characteristics.
 *                  Written to by daemon process.
 *   FILE_ATTR   -- Store information about song attributes, characteristics
 *                  which may change.
 */
#define GJAY_FILE_DATA   "file_data"
#define GJAY_DAEMON_DATA "daemon_data"
#define GJAY_FILE_ATTR   "file_attr"
/* Write the daemon process ID to this file */
#define GJAY_PID       "gjay.pid"

/* We use fixed-size buffers for labels and filenames */
#define BUFFER_SIZE     FILENAME_MAX


#define MIN_PLAYLIST_TIME     10
#define MAX_PLAYLIST_TIME     10000

/* Color wheel size */
#define CATEGORIZE_DIAMETER   200   
#define SELECT_RADIUS         3

/* Size of colorful pixmaps */
#define COLOR_SWATCH_W 27
#define COLOR_SWATCH_H 11
#define FREQ_PM_H      COLOR_SWATCH_H
#define FREQ_PM_W      NUM_FREQ_SAMPLES + 3 + FREQ_PM_H



typedef enum {
    UI = 0,
    DAEMON,
    DAEMON_ATTACHED
} gjay_mode;


/* State */
extern gjay_mode mode;
extern gint      xmms_session;

/* utilities */
void read_line ( FILE * f, char * buffer, int buffer_len);

/* xmms.c */
void        init_xmms   ( void );
void        play_song   ( song * s );
void        play_songs  ( GList * slist );
void        play_files  ( GList * list);

#endif /* GJAY_H */
