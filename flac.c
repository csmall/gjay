/**
 * GJay
 * flac.c - flac file handling
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif


#include <string.h>
#include <stdio.h>
#include <dlfcn.h>

#include <glib.h>
#include <glib/gstdio.h>
#include <FLAC/metadata.h>

#include "flac.h"
#include "gjay.h"
#include "util.h"
#include "i18n.h"

/* the functions */
FLAC__Metadata_Chain* (*gjflac_metadata_chain_new)(void);
FLAC__bool (*gjflac_metadata_chain_read)(FLAC__Metadata_Chain *chain, const char *filename);
FLAC__Metadata_ChainStatus (*gjflac_metadata_chain_status)(FLAC__Metadata_Chain *chain);
void (*gjflac_metadata_chain_delete) (FLAC__Metadata_Chain *chain);

FLAC__Metadata_Iterator* (*gjflac_metadata_iterator_new)(void);
void (*gjflac_metadata_iterator_init)(FLAC__Metadata_Iterator *iterator, FLAC__Metadata_Chain *chain);
FLAC__StreamMetadata * (*gjflac_metadata_iterator_get_block)(FLAC__Metadata_Iterator *iterator);
FLAC__bool (*gjflac_metadata_iterator_next)(FLAC__Metadata_Iterator *iterator);
void (*gjflac_metadata_iterator_delete) (FLAC__Metadata_Iterator *iterator);

gboolean
gjay_flac_dlopen(void) {
  void * lib;

  lib = dlopen("libFLAC.so.8", RTLD_GLOBAL | RTLD_LAZY);
  if (!lib) {
    return FALSE;
  }
  /* Clear any error first */
  dlerror();

  if (gjay_dlsym(lib, &gjflac_metadata_chain_new, "FLAC__metadata_chain_new") == FALSE) return FALSE;
  if (gjay_dlsym(lib, &gjflac_metadata_chain_read, "FLAC__metadata_chain_read") == FALSE) return FALSE;
  if (gjay_dlsym(lib, &gjflac_metadata_chain_status, "FLAC__metadata_chain_status") == FALSE) return FALSE;
  if (gjay_dlsym(lib, &gjflac_metadata_chain_delete, "FLAC__metadata_chain_delete") == FALSE) return FALSE;

  if (gjay_dlsym(lib, &gjflac_metadata_iterator_new, "FLAC__metadata_iterator_new") == FALSE) return FALSE;
  if (gjay_dlsym(lib, &gjflac_metadata_iterator_init, "FLAC__metadata_iterator_init") == FALSE) return FALSE;
  if (gjay_dlsym(lib, &gjflac_metadata_iterator_get_block, "FLAC__metadata_iterator_get_block") == FALSE) return FALSE;
  if (gjay_dlsym(lib, &gjflac_metadata_iterator_next, "FLAC__metadata_iterator_next") == FALSE) return FALSE;
  if (gjay_dlsym(lib, &gjflac_metadata_iterator_delete, "FLAC__metadata_iterator_delete") == FALSE) return FALSE;

  return TRUE;
}

gboolean
read_flac_file_type( gchar    * path,
                      gint     * length,
                      gchar   ** title,
                      gchar   ** artist,
                      gchar   ** album)
{
  FILE *fp;
  int i;

  FLAC__Metadata_Chain *chain;
  assert(gjflac_metadata_chain_new);

  if ( (fp = g_fopen(path, "r")) == NULL)
    return FALSE;

  if ( (chain = (*gjflac_metadata_chain_new)()) == 0)
    return FALSE;

  if (!(*gjflac_metadata_chain_read)(chain, path)) {
    if (verbosity > 2)
      fprintf(stderr, _("Unable to read FLAC metadata for file %s\n"), path);
    return FALSE;
  }
  // Loop through metadata
  FLAC__Metadata_Iterator *iterator = (*gjflac_metadata_iterator_new)();
  FLAC__StreamMetadata *block;
  FLAC__bool ok = true;
  unsigned block_number;

  if (0 == iterator) {
    fprintf(stderr, _("Out of memory creating FLAC iterator.\n"));
    return FALSE;
  }

  (*gjflac_metadata_iterator_init)(iterator, chain);
  block_number = 0;
  do {
    block = (*gjflac_metadata_iterator_get_block)(iterator);
    ok &= (0 != block);
    if (!ok)
      fprintf (stderr, _("Could not get block from FLAC chain for %s\n"), path);
    else {
      // Parse metadata
      switch(block->type) {
      case FLAC__METADATA_TYPE_STREAMINFO:
        // Time is samples / samplerate
        * length = block->data.stream_info.total_samples / block->data.stream_info.sample_rate;
        break;
      case FLAC__METADATA_TYPE_VORBIS_COMMENT:
        for (i = 0; i < block->data.vorbis_comment.num_comments; i++) {
          char* comment = malloc (block->data.vorbis_comment.comments[i].length + 1);
          memcpy (comment, block->data.vorbis_comment.comments[i].entry,  block->data.vorbis_comment.comments[i].length);
          comment[block->data.vorbis_comment.comments[i].length] = '\0';
          //printf ("comment[%u]:%s - %d\n", i, comment, block->data.vorbis_comment.comments[i].length);
        
          if (block->data.vorbis_comment.comments[i].length > 5) {
            if (strncasecmp("ARTIST=", comment, strlen("ARTIST=")) == 0) {
              char* text = malloc (block->data.vorbis_comment.comments[i].length - strlen("ARTIST=") + 1);
              int j;
              for (j = 0; j < block->data.vorbis_comment.comments[i].length - strlen("ARTIST=") + 1; j++) {
                text[j] = block->data.vorbis_comment.comments[i].entry[j + strlen("ARTIST=")];
              }
              text[block->data.vorbis_comment.comments[i].length - strlen("ARTIST=")] = '\0';
              //printf ("ARTIST: %s\n", text);
              *artist = strdup_to_utf8(text);
              free (text);
            }

            if (strncasecmp("TITLE=", comment, strlen("TITLE=")) == 0) {
            char* text = malloc (block->data.vorbis_comment.comments[i].length - strlen("TITLE=") + 1);
            int j;
            for (j = 0; j < block->data.vorbis_comment.comments[i].length - strlen("TITLE=") + 1; j++) {
              text[j] = block->data.vorbis_comment.comments[i].entry[j + strlen("TITLE=")];
            }
            text[block->data.vorbis_comment.comments[i].length - strlen("TITLE=")] = '\0';
            //printf ("TITLE: %s\n", text);
            *title = strdup_to_utf8(text);
            free (text);
          }

          if (strncasecmp("ALBUM=", comment, strlen("ALBUM=")) == 0) {
            char* text = malloc (block->data.vorbis_comment.comments[i].length - strlen("ALBUM=") + 1);
            int j;
            for (j = 0; j < block->data.vorbis_comment.comments[i].length - strlen("ALBUM=") + 1; j++) {
              text[j] = block->data.vorbis_comment.comments[i].entry[j + strlen("ALBUM=")];
            }
            text[block->data.vorbis_comment.comments[i].length - strlen("ALBUM=")] = '\0';
            //printf ("ALBUM: %s\n", text);
            *album = strdup_to_utf8(text);
            free (text);
          }

        }
        
        free (comment);
      }
      break;
    default:
      // Ignore all other metadata blocks
      break;
      }
      block_number++;
    } 
  } while (ok && (*gjflac_metadata_iterator_next)(iterator));
  (*gjflac_metadata_iterator_delete) (iterator);
  (gjflac_metadata_chain_delete) (chain);
      
  fclose(fp);

  return TRUE;
}


