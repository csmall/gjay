#ifndef __CONSTANTS_H__
#define __CONSTANTS_H__

/* Default directory for storing app info */
#define GJAY_VERSION        "0.2.6"
#define GJAY_DIR            ".gjay"
#define GJAY_PREFS          "prefs.xml"
#define GJAY_FILE_DATA      "data.xml"
#define GJAY_DAEMON_DATA    "daemon.xml"
#define GJAY_QUEUE          "analysis_queue"
#define GJAY_TEMP           "temp_analysis_append"
#define GJAY_PID            "gjay.pid"

/* We use fixed-size buffers for labels and filenames */
#define BUFFER_SIZE          FILENAME_MAX

/* Color wheel size */
#define CATEGORIZE_DIAMETER  200   
#define SELECT_RADIUS        3

/* Values */
#define MAX_FREQ_VAL         10.0
#define MAX_VOL_DIFF         10.0
#define MIN_CRITERIA         0.1
#define MAX_CRITERIA         10.0
#define MIN_RATING           0.0
#define MAX_RATING           5.0
#define MIN_BPM              100.0
#define MAX_BPM              160.0
#define MAX_HUE              2*M_PI
#define MAX_BRIGHTNESS       1.0
#define DEFAULT_CRITERIA     5.0
#define DEFAULT_RATING       2.5

/* We batch the freq spectrum into just a few bins */
#define NUM_FREQ_SAMPLES     30

/* Periodically write changed song data to disk */
#define SONG_DIRTY_WRITE_TIMEOUT  1000 * 60 * 2

/* For prefs */
#define DEFAULT_PLAYLIST_TIME 72

#endif /* __CONSTANTS_H__ */