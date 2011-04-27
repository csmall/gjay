#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.62])
AC_INIT([gjay],[0.3.2],[csmall@enc.com.au])
AC_CONFIG_SRCDIR([gjay.h])
AC_CONFIG_HEADERS([config.h])
AM_INIT_AUTOMAKE([1.10])

dnl Options
AC_SUBST([WITH_MPDCLIENT])
AC_ARG_ENABLE([mpdclient],
              AS_HELP_STRING([--with-mpdclient], [Enable mpd  client(default is YES)]),
              [enable_mpdclient=$enableval],
              [enable_mpdclient="yes"])
if test "$enable_mpdclient" = "yes"; then
  AC_DEFINE([WITH_MPDCLIENT], [1], [Enable mpd music player])
  PKG_CHECK_MODULES([MPDCLIENT], [libmpdclient])
fi


# Checks for programs.
AC_PROG_CC
AC_PROG_INSTALL
PKG_PROG_PKG_CONFIG

dnl Check for language stuff
AM_GNU_GETTEXT([external])
AM_GNU_GETTEXT_VERSION([0.18.1])

# Checks for libraries.
AM_PATH_GTK_2_0
PKG_CHECK_MODULES([DBUS_GLIB], [dbus-glib-1])
PKG_CHECK_MODULES([AUDACIOUS], [audacious])

AX_PATH_GSL
dnl AC_CHECK_LIB([audclient], [audacious_remote_playlist])
AC_CHECK_LIB([dl], [dlopen])

# Checks for header files.
#AC_CHECK_HEADER([FLAC/metadata.h], [], [AC_MSG_WARN(No FLAC header found: FLAC support will not be compilied in)])
AC_CHECK_HEADERS([FLAC/metadata.h vorbis/vorbisfile.h])
#AC_CHECK_HEADERS([fcntl.h limits.h stdint.h stdlib.h string.h strings.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
#AC_TYPE_INT16_T
#AC_TYPE_OFF_T
#AC_TYPE_PID_T
#AC_TYPE_SIZE_T
#AC_TYPE_UINT16_T
#AC_TYPE_UINT32_T

# Checks for library functions.
#AC_FUNC_FORK
#AC_FUNC_MALLOC
#AC_CHECK_FUNCS([bzero floor memset mkdir rmdir sqrt strcasecmp strdup strerror strncasecmp strtol])

AC_CONFIG_FILES([Makefile po/Makefile.in
                 doc/Makefile
                 icons/Makefile])
AC_OUTPUT
