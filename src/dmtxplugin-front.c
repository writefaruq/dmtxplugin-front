/***************************************************************************
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
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "symbol.h"
#include "utils.h"
#include "dmtxplugin-front.h"

void handle_file_creation( char *infile )
{
        int img_count = 0;
        char *outfile = DMTX_SYMBOL_OUTPUT;

        img_count = symbol_decode(infile, outfile);
        printf(" from %s to %s, %d symbol decoded \n", infile, outfile, img_count);

        if ( img_count == 1) { /* assuming successful decode */
                log_message(LOG_FILE, "Decoded dmtx symbol\n");
                dmtxplugin_gdbus_create_device(outfile);
        } else {
                log_message(LOG_FILE, "failed to decode dmtx symbol\n");
        }
}

int main (int argc, char *argv[])
{
        handle_file_creation(argv[1]);

        return 0;
}

