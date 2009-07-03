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

#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/inotify.h>

#include "symbol.h"
#include "utils.h"
#include "inotify.h"

#define EVENT_SIZE  ( sizeof (struct inotify_event) )
#define BUF_LEN     ( 10 * 1024 * ( EVENT_SIZE + 16 ) )

void handle_file_creation(char *infile)
{
        int img_count = 0;
        char *outfile = DMTX_SYMBOL_OUTPUT;
        img_count = symbol_decode(infile, outfile);
        //printf(" from %s to %s, %d symbol decoded \n", infile, outfile, img_count);
        if ( img_count == 1) { /* assuming successful decode */
                log_message(LOG_FILE, "Decoded dmtx symbol\n");
                dmtxplugin_gdbus_create_device(outfile);
        } else {
                log_message(LOG_FILE, "failed to decode dmtx symbol\n");
        }
}

int inotify_watcher(void)
{
	int err, length, i = 0, j = 0;
	int fd;
	int wd;
	char buffer[BUF_LEN];
	char *infile;

	fd = inotify_init();

	if (fd < 0) {
		/* FIXME: Missing return? */
		err = errno;
		perror("inotify_init");
		log_message(LOG_FILE, "inotify_init err\n");
		return err;
	}

        log_message(LOG_FILE, "inotify_init: OK\n");

	wd = inotify_add_watch(fd, DMTX_SYMBOLDIR, IN_MODIFY | IN_CREATE | IN_DELETE);
	length = read(fd, buffer, BUF_LEN);

	if (length < 0) {
		/* FIXME: Missing return? */
		err = errno;
		log_message(LOG_FILE, "inotify read err\n");
		perror("read");
		return err;
	}

	log_message(LOG_FILE, "inotify read: OK\n");

	/* FIXME: Please avoid nested if */
	while (i < length) {
		struct inotify_event *event = (struct inotify_event *) &buffer[i];
		if (event->len) {
		        switch (event->mask) {
		                case IN_CREATE:
		                if (event->mask & IN_ISDIR) {
		                        printf("The directory %s was created.\n", event->name);
		                } else {
		                        //log_message(LOG_FILE, "The file was created.\n");
		                        //printf("The file %s was created.\n", event->name);
					/* Jump to symbol decode code*/
					sprintf(infile, "%s", event->name);
					log_message(LOG_FILE, infile);
					handle_file_creation(infile);
					log_message(LOG_FILE, "After file handled.\n");
		                }
		                break;
		                case IN_DELETE:
		                //if (event->mask & IN_ISDIR)
					//log_message("The directory %s was deleted.\n", event->name);
				//else
					//log_message("The file %s was deleted.\n", event->name);
		                break;
                                case IN_MODIFY:
                                //if (event->mask & IN_ISDIR)
					//log_message("The directory %s was modified.\n", event->name);
				//else
					//log_message("The file %s was modified.\n", event->name);
		                break;
		                default:
		                log_message(LOG_FILE, "Event is not interesting to us.\n");
		                //log_message("Event %s is not interesting to us.\n", event->name);

		        }
		}
		i += EVENT_SIZE + event->len;
	}

	(void) inotify_rm_watch(fd, wd);
	(void) close(fd);

        log_message(LOG_FILE, "inotify close: OK");
	/* FIXME: exit is not correct */
	return 0;
}
