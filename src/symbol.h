/***************************************************************************
 *       Dmtx-Plugin-Front                                                 *
 *       Author: Md Omar Faruque Sarker <writefaruq@gmail.com>             *
 *       libdmtx - Data Matrix Encoding/Decoding Library                   *
 *       Copyright (C) 2008, 2009 Mike Laughton                            *
 *       Copyright (C) 2008 Ryan Raasch                                    *
 *       Copyright (C) 2008 Olivier Guilyardi                              *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.              *
***************************************************************************/
#ifndef __DMTXSYMBOL_H__
#define __DMTXSYMBOL_H__

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <getopt.h>
#include <errno.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <math.h>
#include <stdarg.h>
#include <assert.h>
#include <wand/magick-wand.h>
#include <dmtx.h>
#include "dmtxutil.h"

#if ENABLE_NLS
# include <libintl.h>
# define _(String) gettext(String)
#else
# define _(String) String
#endif
#define N_(String) String

#define DMTXWRITE_BUFFER_SIZE 4096

typedef struct {
   int codewords;       /* -c, --codewords */
   int edgemin;         /* -e, --minimum-edge */
   int edgemax;         /* -E, --maximum-edge */
   int scangap;         /* -g, --gap */
   int timeout;       /* -m, --milliseconds */
   int newline;         /* -n, --newline */
   int page;            /* -p, --page */
   int square_devn;      /* -q, --square-deviation */
   int dpi;             /* -r, --resolution */
   int size_idx_expected; /* -s, --symbol-size */
   int edge_thresh;      /* -t, --threshold */
   char *xmin;          /* -x, --x-range-min */
   char *xmax;          /* -X, --x-range-max */
   char *ymin;          /* -y, --y-range-min */
   char *ymax;          /* -Y, --y-range-max */
   int corrections_max;  /* -C, --corrections-max */
   int diagnose;        /* -D, --diagnose */
   int mosaic;          /* -M, --mosaic */
   int stop_after;       /* -N, --stop-after */
   int page_numbers;     /* -P, --page-numbers */
   int corners;         /* -R, --corners */
   int shrink_max;       /* -S, --shrink */
   int shrink_min;       /* -S, --shrink (if range specified) */
   int unicode;         /* -U, --unicode */
   int verbose;         /* -v, --verbose */
} DecodeOptions;

int symbol_decode(char *infile, char *outfile);

/* Internal functions  for decoding a symbol */
DecodeOptions get_default_decode_options(void);
DmtxPassFail set_decode_options(DmtxDecode *dec, DmtxImage *img, DecodeOptions *opt);
DmtxPassFail print_stats(DmtxDecode *dec, DmtxRegion *reg, DmtxMessage *msg,
        int img_page_index, DecodeOptions *opt);
DmtxPassFail print_message(DmtxRegion *reg, DmtxMessage *msg, DecodeOptions *opt,
        const char *outfile);
void write_diagnostic_image(DmtxDecode *dec, char *imagePath);
int scale_number_string(char *s, int extent);
void cleanup_magick(MagickWand **wand, int magicError);

#endif
