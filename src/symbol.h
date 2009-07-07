/***************************************************************************
Bluez-Dmtx Plugin Symbol interface
Copyright (C) 2009, M Omar Faruque Sarker

libdmtx - Data Matrix Encoding/Decoding Library
Copyright (C) 2008, 2009 Mike Laughton
Copyright (C) 2008 Olivier Guilyardi

BlueZ - Bluetooth protocol stack for Linux
Copyright (C) 2004-2009  Marcel Holtmann <marcel@holtmann.org>

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

Contact: writefaruq@gmail.com
*/
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
   int edgeMin;         /* -e, --minimum-edge */
   int edgeMax;         /* -E, --maximum-edge */
   int scanGap;         /* -g, --gap */
   int timeoutMS;       /* -m, --milliseconds */
   int newline;         /* -n, --newline */
   int page;            /* -p, --page */
   int squareDevn;      /* -q, --square-deviation */
   int dpi;             /* -r, --resolution */
   int sizeIdxExpected; /* -s, --symbol-size */
   int edgeThresh;      /* -t, --threshold */
   char *xMin;          /* -x, --x-range-min */
   char *xMax;          /* -X, --x-range-max */
   char *yMin;          /* -y, --y-range-min */
   char *yMax;          /* -Y, --y-range-max */
   int correctionsMax;  /* -C, --corrections-max */
   int diagnose;        /* -D, --diagnose */
   int mosaic;          /* -M, --mosaic */
   int stopAfter;       /* -N, --stop-after */
   int pageNumbers;     /* -P, --page-numbers */
   int corners;         /* -R, --corners */
   int shrinkMax;       /* -S, --shrink */
   int shrinkMin;       /* -S, --shrink (if range specified) */
   int unicode;         /* -U, --unicode */
   int verbose;         /* -v, --verbose */
} DecodeOptions;

typedef struct {
   char *inputPath;
   char *outputPath;
   char *format;
   int codewords;
   int marginSize;
   int moduleSize;
   int scheme;
   int preview;
   int rotate;
   int sizeIdx;
   int color[3];
   int bgColor[3];
   int mosaic;
   int dpi;
   int verbose;
} EncodeOptions;

int symbol_decode(char *infile, char *outfile);

/* Internal functions  for decoding a symbol */
DecodeOptions GetDefaultDecodeOptions(void);

/* DmtxPassFail HandleDecodeArgs(DecodeOptions *opt, int *fileIndex, int *argcp, char **argvp[]);
*/
DmtxPassFail SetDecodeOptions(DmtxDecode *dec, DmtxImage *img, DecodeOptions *opt);

DmtxPassFail PrintStats(DmtxDecode *dec, DmtxRegion *reg, DmtxMessage *msg,
      int imgPageIndex, DecodeOptions *opt);

DmtxPassFail PrintMessage(DmtxRegion *reg, DmtxMessage *msg, DecodeOptions *opt, const char *outfile);

void WriteDiagnosticImage(DmtxDecode *dec, char *imagePath);

int ScaleNumberString(char *s, int extent);

/* Utility functions */
void CleanupMagick(MagickWand **wand, int magicError);

void ListImageFormats(void);

#endif
