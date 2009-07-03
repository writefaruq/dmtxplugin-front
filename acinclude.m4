AC_DEFUN([AC_PATH_LIBDMTX], [
    AC_CHECK_LIB(dmtx, dmtxEncodeCreate, dummy=yes,
				AC_MSG_ERROR(libdmtx => 0.7 is required))
    PKG_CHECK_MODULES(
            MAGICK,
            Wand >= 6.2.4,
            true,
            AC_MSG_ERROR(dmtx plug-in requires Wand >= 6.2.4))
    AC_SUBST(MAGICK_CFLAGS)
    AC_SUBST(MAGICK_LIBS)
    AC_SUBST(LIBDMTX_CFLAGS)
    AC_SUBST(LIBDMTX_LIBS)
])

AC_DEFUN([AC_PATH_DBUS], [
	PKG_CHECK_MODULES(DBUS, dbus-1 >= 1.0, dummy=yes,
				AC_MSG_ERROR(D-Bus library is required))
	AC_CHECK_LIB(dbus-1, dbus_watch_get_unix_fd, dummy=yes,
		AC_DEFINE(NEED_DBUS_WATCH_GET_UNIX_FD, 1,
			[Define to 1 if you need the dbus_watch_get_unix_fd() function.]))
	AC_SUBST(DBUS_CFLAGS)
	AC_SUBST(DBUS_LIBS)
])

AC_DEFUN([AC_PATH_GLIB], [
	PKG_CHECK_MODULES(GLIB, glib-2.0 >= 2.14, dummy=yes,
				AC_MSG_ERROR(GLib library version 2.14 or later is required))
	AC_SUBST(GLIB_CFLAGS)
	AC_SUBST(GLIB_LIBS)
])

AC_DEFUN([AC_PATH_BLUEZ], [
	PKG_CHECK_MODULES(BLUEZ, bluez >= 4.40, dummy=yes,
				AC_MSG_ERROR(Bluez library version 4.40 or later is required))
	AC_SUBST(BLUEZ_CFLAGS)
	AC_SUBST(BLUEZ_LIBS)
])

AC_DEFUN([AC_PATH_GDBUS], [
        AC_SUBST([GDBUS_CFLAGS], ['-I$(top_srcdir)/gdbus'])
        AC_SUBST([GDBUS_LIBS], ['$(top_builddir)/gdbus/libgdbus.la'])
])
