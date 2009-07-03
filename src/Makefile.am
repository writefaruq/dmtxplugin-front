#AM_CFLAGS = -Wall -g

bin_PROGRAMS = dmtxpluginfront

dmtxpluginfront_SOURCES = dmtxplugin-front.c dmtxplugin-front.h symbol.c symbol.h dmtxutils.c \
		dmtxutils.h  dmtxplugin-gdbus.c dmtxplugin-gdbus.h \
		service-gdbus.c service-gdbus.h utils.c utils.h

AM_CFLAGS = -Wall -g -I/usr/local/include -I/usr/include -D_MAGICK_CONFIG_H \
                @MAGICK_CFLAGS@ @BLUEZ_CFLAGS@ @DBUS_CFLAGS@ @GLIB_CFLAGS@ \
                @GDBUS_CFLAGS@

dmtxpluginfront_LDFLAGS = -ldmtx @LIBDMTX_CFLAGS@ @MAGICK_LIBS@ \
        @GDBUS_LIBS@ @GLIB_LIBS@ @DBUS_LIBS@ @BLUEZ_LIBS@ @MAGICK_LIBS@

LD_ADD = @LIBDMTX_CFLAGS@ @MAGICK_LIBS@ \
        @GDBUS_LIBS@ @GLIB_LIBS@ @DBUS_LIBS@ @BLUEZ_LIBS@ @MAGICK_LIBS@
