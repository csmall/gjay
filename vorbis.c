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
 * Thin wrapper for libvorbis and libvorbisfile, which are dlopen'ed. 
 */
#include <dlfcn.h>
#include "vorbis.h"

int    vorbis_opened = 0;
char * vorbis_error = NULL;

/* Local null-behavior */
int local_ov_open    (FILE *f, void * vf, char *initial, long ibytes) {
    return 0;
}


vorbis_comment * local_ov_comment  (void * vf, int link) {
    return NULL;
}

double local_ov_time_total (void * vf, int i) {
    return 0;
}

int local_ov_clear (void * vf) {
    return 0;
}


ov_open gj_ov_open = local_ov_open;
ov_comment gj_ov_comment = local_ov_comment;
ov_time_total gj_ov_time_total = local_ov_time_total;
ov_clear gj_ov_clear = local_ov_clear;

char * gjay_vorbis_error(void) {
    return vorbis_error;
}

int gjay_vorbis_available(void) {
    return vorbis_opened;
}


int gjay_vorbis_dlopen(void) {
    void * lib;
    void * sym;
    
    vorbis_opened = 0;
    lib = dlopen("libvorbis.so.0", RTLD_GLOBAL | RTLD_LAZY);
    if (!lib) {
        vorbis_error = "Unable to open libvorbis.so.0";
        return vorbis_opened;
    }

    lib = dlopen("libvorbisfile.so.3", RTLD_GLOBAL | RTLD_LAZY);
    if (lib) {
        vorbis_opened = 1;
        
        sym = dlsym(lib, "ov_open");
        if (sym) {
            gj_ov_open = (ov_open) sym;
        } else {
            vorbis_error = "Did not find symbol: ov_open";
            vorbis_opened = 0;
        }

        sym = dlsym(lib, "ov_comment");
        if (sym) {
            gj_ov_comment = (ov_comment) sym;
        } else {
            vorbis_error = "Did not find symbol: ov_comment";
            vorbis_opened = 0;
        }

        sym = dlsym(lib, "ov_time_total");
        if (sym) {
            gj_ov_time_total = (ov_time_total) sym;
        } else {
            vorbis_error = "Did not find symbol: ov_time_total";
            vorbis_opened = 0;
        }

        sym = dlsym(lib, "ov_clear");
        if (sym) {
            gj_ov_clear = (ov_clear) sym;
        } else {
            vorbis_error = "Did not find symbol: ov_clear";
            vorbis_opened = 0;
        }
    } else {
        vorbis_error = "Unable to open libvorbisfile.so";
    }
    return vorbis_opened;
}


