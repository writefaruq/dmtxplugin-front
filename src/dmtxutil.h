/*
libdmtx - Data Matrix Encoding/Decoding Library

Copyright (C) 2008, 2009 Mike Laughton

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

Contact: mike@dragonflylogic.com
*/

/* $Id: dmtxread.h 124 2008-04-13 01:38:03Z mblaughton $ */

#ifndef __DMTXUTIL_H__
#define __DMTXUTIL_H__

#ifdef HAVE_SYSEXITS_H
#include <sysexits.h>
#else
#define EX_OK           0
#define EX_USAGE       64
#define EX_DATAERR     65
#define EX_SOFTWARE    70
#define EX_OSERR       71
#define EX_CANTCREAT   73
#define EX_IOERR       74
#endif

extern DmtxPassFail string_to_int(int *numberInt, char *number_string, char **terminate);
extern void fatal_error(int error_code, char *fmt, ...);
extern char *basename(char *path);

#endif
