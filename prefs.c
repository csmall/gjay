/**
 * GJay, copyright (c) 2002 Chuck Groom
 *
 * The preference XML format is:
 * 
 * <rootdir [extension_filer="t"]>Path</rootdir>
 * <flags [hide_tip=...] [wander=...]>
 * <daemon_action>enum</daemon_action>
 * <start>random|selected|color</start>
 * <selection_limit>songs|dir<selection_limit>
 * <rating [cutoff=...]>float</rating>
 * <hue>...
 * <brightness>...
 * <bpm>...
 * <freq>...
 * <variance>...
 * <path_weight>...
 * <color>float float</color>
 * <time>int</time>
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

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h> 
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include "gjay.h"

typedef enum {
    PE_ROOTDIR,
    PE_FLAGS,
    PE_DAEMON_ACTION,
    PE_START,
    PE_SELECTION_LIMIT,
    PE_RATING,
    PE_HUE,
    PE_BRIGHTNESS,
    PE_BPM,
    PE_FREQ,
    PE_VARIANCE,
    PE_PATH_WEIGHT,
    PE_COLOR,
    PE_TIME,
    /* attributes */
    PE_EXTENSION_FILTER,
    PE_HIDE_TIP,
    PE_WANDER,
    PE_CUTOFF,
    /* values */
    PE_RANDOM,
    PE_SELECTED,
    PE_SONGS,
    PE_DIR, 
    PE_LAST
} pref_element_type;


char * pref_element_strs[PE_LAST] = {
    "rootdir",
    "flags",
    "daemon_action",
    "start",
    "selection_limit",
    "rating",
    "hue",
    "brightness",
    "bpm",
    "freq",
    "variance",
    "path_weight",
    "color",
    "time",
    "extension_filter",
    "hide_tip",
    "wander",
    "cutoff",
    "random",
    "selected",
    "songs",
    "dir"
};


app_prefs prefs;


static void     data_start_element  ( GMarkupParseContext *context,
                                      const gchar         *element_name,
                                      const gchar        **attribute_names,
                                      const gchar        **attribute_values,
                                      gpointer             user_data,
                                      GError             **error );
static void     data_text           ( GMarkupParseContext *context,
                                      const gchar         *text,
                                      gsize                text_len,  
                                      gpointer             user_data,
                                      GError             **error );
static int      get_element         ( gchar * element_name );


void load_prefs ( void ) {
    GMarkupParseContext * parse_context;
    GMarkupParser parser;
    gboolean result = TRUE;
    GError * error;
    char buffer[BUFFER_SIZE];
    FILE * f;
    gssize text_len;
    pref_element_type element;

    /* Set default values */
    memset(&prefs, 0x00, sizeof(app_prefs));
    prefs.rating = DEFAULT_RATING;
    prefs.time = DEFAULT_PLAYLIST_TIME;
    prefs.variance =
        prefs.hue = 
        prefs.brightness =
        prefs.bpm =
        prefs.freq =
        prefs.path_weight = DEFAULT_CRITERIA;
    prefs.extension_filter = TRUE;
    prefs.color.H = 0;
    prefs.color.B = 0.5;
    prefs.daemon_action = PREF_DAEMON_QUIT;
    prefs.hide_tips = FALSE;
    snprintf(buffer, BUFFER_SIZE, "%s/%s/%s", getenv("HOME"), 
             GJAY_DIR, GJAY_PREFS);

    f = fopen(buffer, "r");
    if (f) {
        parser.start_element = data_start_element;
        parser.text = data_text;
        parser.end_element = NULL; 

        parse_context = g_markup_parse_context_new(&parser, 0, &element, NULL);
        while (result && !feof(f)) {
            text_len = fread(buffer, 1, BUFFER_SIZE, f);
            result = g_markup_parse_context_parse ( parse_context,
                                                    buffer,
                                                    text_len,
                                                    &error);
            error = NULL;
        }
        g_markup_parse_context_free(parse_context);
        fclose(f);
    } 
}



void save_prefs ( void ) {
    char buffer[BUFFER_SIZE], buffer_temp[BUFFER_SIZE];
    char * utf8;
    FILE * f;
    
    snprintf(buffer, BUFFER_SIZE, "%s/%s/%s", getenv("HOME"), 
             GJAY_DIR, GJAY_PREFS);
    snprintf(buffer_temp, BUFFER_SIZE, "%s_temp", buffer);
    f = fopen(buffer_temp, "w");
    if (f) {
        fprintf(f, "<gjay_prefs>\n");
        if (prefs.song_root_dir) {
            fprintf(f, "<%s", pref_element_strs[PE_ROOTDIR]);
            if (prefs.extension_filter)
                fprintf(f, " %s=\"t\"", pref_element_strs[PE_EXTENSION_FILTER]);
            utf8 = strdup_to_utf8(prefs.song_root_dir);
            fprintf(f, ">%s</%s>\n", 
                    utf8,
                    pref_element_strs[PE_ROOTDIR]);
            g_free(utf8);
        }
        fprintf(f, "<%s", pref_element_strs[PE_FLAGS]);
        if (prefs.hide_tips)
            fprintf(f, " %s=\"t\"", pref_element_strs[PE_HIDE_TIP]);
        if (prefs.wander)
            fprintf(f, " %s=\"t\"", pref_element_strs[PE_WANDER]);
        fprintf(f, "></%s>\n", pref_element_strs[PE_FLAGS]);
        
        fprintf(f, "<%s>%d</%s>\n", 
                pref_element_strs[PE_DAEMON_ACTION],
                prefs.daemon_action,
                pref_element_strs[PE_DAEMON_ACTION]);

        fprintf(f, "<%s>", pref_element_strs[PE_START]);
        if (prefs.start_selected)
            fprintf(f, "%s", pref_element_strs[PE_SELECTED]);
        else if (prefs.start_color)
            fprintf(f, "%s", pref_element_strs[PE_COLOR]);
        else
            fprintf(f, "%s", pref_element_strs[PE_RANDOM]);
        fprintf(f, "</%s>\n", pref_element_strs[PE_START]);

        fprintf(f, "<%s>", pref_element_strs[PE_SELECTION_LIMIT]);        
        if (prefs.use_selected_songs)
            fprintf(f, "%s", pref_element_strs[PE_SONGS]);
        else if (prefs.use_selected_dir) 
            fprintf(f, "%s", pref_element_strs[PE_DIR]);
        fprintf(f, "</%s>\n", pref_element_strs[PE_SELECTION_LIMIT]);        
        
        fprintf(f, "<%s>%d</%s>\n",
                pref_element_strs[PE_TIME],
                prefs.time,
                pref_element_strs[PE_TIME]);

        fprintf(f, "<%s>%f %f</%s>\n",
                pref_element_strs[PE_COLOR],
                prefs.color.H,
                prefs.color.B,
                pref_element_strs[PE_COLOR]);
                
        if (prefs.rating_cutoff)
            fprintf(f, "<%s %s=\"t\">", 
                    pref_element_strs[PE_RATING],
                    pref_element_strs[PE_CUTOFF]);
        else
            fprintf(f, "<%s>", pref_element_strs[PE_RATING]);
        fprintf(f, "%f", prefs.rating);
        fprintf(f, "</%s>\n", pref_element_strs[PE_RATING]);
        
        fprintf(f, "<%s>%f</%s>\n",
                pref_element_strs[PE_VARIANCE],
                prefs.variance,
                pref_element_strs[PE_VARIANCE]);

        fprintf(f, "<%s>%f</%s>\n",
                pref_element_strs[PE_HUE],
                prefs.hue,
                pref_element_strs[PE_HUE]);

        fprintf(f, "<%s>%f</%s>\n",
                pref_element_strs[PE_BRIGHTNESS],
                prefs.brightness,
                pref_element_strs[PE_BRIGHTNESS]);

        fprintf(f, "<%s>%f</%s>\n",
                pref_element_strs[PE_BPM],
                prefs.bpm,
                pref_element_strs[PE_BPM]);

        fprintf(f, "<%s>%f</%s>\n",
                pref_element_strs[PE_FREQ],
                prefs.freq,
                pref_element_strs[PE_FREQ]);

        fprintf(f, "<%s>%f</%s>\n",
                pref_element_strs[PE_PATH_WEIGHT],
                prefs.path_weight,
                pref_element_strs[PE_PATH_WEIGHT]);

        fprintf(f, "</gjay_prefs>\n");
        fclose(f);
        rename(buffer_temp, buffer);
    } else {
        fprintf(stderr, "Unable to write prefs %s\n", buffer);
    }
}


/* Called for open tags <foo bar="baz"> */
void data_start_element  (GMarkupParseContext *context,
                          const gchar         *element_name,
                          const gchar        **attribute_names,
                          const gchar        **attribute_values,
                          gpointer             user_data,
                          GError             **error) {
    gint k;
    pref_element_type * element = (pref_element_type *) user_data;
    pref_element_type attr;
    *element = get_element((char *) element_name);
    
    for (k = 0; attribute_names[k]; k++) {
        attr = get_element((char *) attribute_names[k]);
        switch(attr) {
        case PE_EXTENSION_FILTER:
            if (*element == PE_ROOTDIR)
                prefs.extension_filter = TRUE;
            break;
        case PE_HIDE_TIP:
            if (*element == PE_FLAGS)
                prefs.hide_tips = TRUE;
            break;
        case PE_WANDER:
            if (*element == PE_FLAGS)
                prefs.wander = TRUE;
            break;
        case PE_CUTOFF:
            if (*element == PE_RATING)
                prefs.rating_cutoff = TRUE;
            break;
        default:
        }
    }
}    

void data_text ( GMarkupParseContext *context,
                 const gchar         *text,
                 gsize                text_len,  
                 gpointer             user_data,
                 GError             **error) {
    gchar buffer[BUFFER_SIZE];
    gchar * buffer_str;
    pref_element_type * element = (pref_element_type *) user_data;    
    pref_element_type val;

    memcpy(buffer, text, text_len);
    buffer[text_len] = '\0';

    switch(*element) {
    case PE_ROOTDIR:
        prefs.song_root_dir = strdup_to_latin1(buffer); 
        break;
    case PE_DAEMON_ACTION:
        prefs.daemon_action = atoi(buffer);
        break;
    case PE_START:
    case PE_SELECTION_LIMIT:
        val = get_element((char *) buffer);
        switch(val) {
        case PE_SELECTED:
            prefs.start_selected = TRUE;
            break;
        case PE_COLOR:
            prefs.start_color = TRUE;
            break;
        case PE_SONGS:
            prefs.use_selected_songs = TRUE;
            break;  
        case PE_DIR:
            prefs.use_selected_dir = TRUE;
            break;
        case PE_RANDOM:
            /* Don't do anything */
        default:
            break;
        }
        break;
    case PE_RATING:
        prefs.rating = strtod(buffer, NULL);
        break;
    case PE_HUE:
        prefs.hue = strtod(buffer, NULL);
        break;
    case PE_BRIGHTNESS:
        prefs.brightness = strtod(buffer, NULL);
        break;
    case PE_BPM:
        prefs.bpm = strtod(buffer, NULL);
        break;
    case PE_FREQ:
        prefs.freq = strtod(buffer, NULL);
        break;
    case PE_VARIANCE:
        prefs.variance = strtod(buffer, NULL);
        break;
    case PE_PATH_WEIGHT:
        prefs.path_weight = strtod(buffer, NULL);  
        break;
    case PE_COLOR:
        prefs.color.H = strtod(buffer, &buffer_str);
        prefs.color.B = strtod(buffer_str, NULL);
        break;
    case PE_TIME:
        prefs.time = atoi(buffer);
        break;
    default:
    }
    *element = PE_LAST;
}

static int get_element ( gchar * element_name ) {
    int k;
    for (k = 0; k < PE_LAST; k++) {
        if (strcasecmp(pref_element_strs[k], element_name) == 0)
            return k;
    }
    return k;
}
