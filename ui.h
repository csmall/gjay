#ifndef __UI_H__ 
#define __UI_H__ 

#include "gjay.h"

#define APP_WIDTH  600
#define APP_HEIGHT 420
#define PLAYLIST_WIDTH  500
#define PLAYLIST_HEIGHT 250
#define MSG_WIDTH  250
#define MSG_HEIGHT 160
#define FREQ_IMAGE_WIDTH    NUM_FREQ_SAMPLES
#define FREQ_IMAGE_HEIGHT   15
#define COLOR_IMAGE_WIDTH   FREQ_IMAGE_WIDTH
#define COLOR_IMAGE_HEIGHT  FREQ_IMAGE_HEIGHT
#define COLORWHEEL_DIAMETER 150
#define COLORWHEEL_SELECT   3


typedef enum {
    TAB_EXPLORE = 0,
    TAB_PLAYLIST,
    TAB_PREFS,
    TAB_ABOUT,
    TAB_LAST
} tab_val;

typedef enum {
    PM_FILE_PENDING = 0,
    PM_FILE_PENDING2,
    PM_FILE_PENDING3,
    PM_FILE_PENDING4,
    PM_FILE_NOSONG,
    PM_FILE_SONG,
    PM_DIR_OPEN,
    PM_DIR_CLOSED,
    PM_ICON_PENDING,
    PM_ICON_NOSONG,
    PM_ICON_SONG,
    PM_ICON_OPEN,
    PM_ICON_CLOSED,
    PM_BUTTON_PLAY,
    PM_BUTTON_DIR,
    PM_BUTTON_ALL,
    PM_BUTTON_ALL_RECURSIVE,
    PM_STAR,
    PM_STAR_SMALL,
    PM_ABOUT,
    PM_COLOR_SEL,
    PM_NOT_SET,
    PM_LAST
} pm;

extern GdkPixbuf * pixbufs[PM_LAST];

extern GtkWidget * window;
extern GtkWidget * notebook;
extern GtkWidget * explore_view, * selection_view, * playlist_view,
                 * no_root_view, * prefs_view, * about_view;
extern GList     * selected_songs, * selected_files; 
extern int         tree_depth;

/* UI utils */
GtkWidget * new_button_label_pixbuf      ( char * label_text, 
                                           int pm_type);
void        switch_page (GtkNotebook *notebook,
                         GtkNotebookPage *page,
                         gint page_num,
                         gpointer user_data);
GtkWidget * make_app_ui     ( void );
void        display_message ( gchar * msg );


GtkWidget * make_explore_view ( void );
GtkWidget * make_playlist_view ( void );
GtkWidget * make_no_root_view ( void );
GtkWidget * make_selection_view ( void );
GtkWidget * make_prefs_view ( void );
GtkWidget * make_about_view ( void );


/* Explore files pane  */
void        explore_view_set_root        ( char * root_dir );
gboolean    explore_update_file_pm       ( char * file, 
                                           int type );
GList *     explore_files_in_dir         ( char * dir, 
                                           gboolean recursive );
gint        explore_files_depth_distance ( char * file1, 
                                           char * file2 );
void        explore_animate_pending      ( char * file );
void        explore_animate_stop         ( void );


/* Select file pane */
void        set_selected_file            ( char * file, 
                                           char * short_name, 
                                           gboolean is_dir );
void        update_selected_songs_color  ( gdouble angle, 
                                           gdouble radius );
GtkWidget * create_colorwheel            ( gint diameter,
                                           GList ** list,
                                           HB * color );
#endif /* __UI_H__ */



