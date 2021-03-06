#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.62])
AC_INIT([gjay],[0.4],[csmall@enc.com.au])
AC_CONFIG_SRCDIR([gjay.h])
AC_CONFIG_HEADERS([config.h])
AM_INIT_AUTOMAKE([1.10])

# Checks for programs.
AC_PROG_CC
AC_PROG_INSTALL
PKG_PROG_PKG_CONFIG

dnl Options
# Don't require D-Bus unless building a component that needs it:
enable_dbus=no

AC_SUBST([WITH_GUI])
AC_ARG_ENABLE([gui],
			  AS_HELP_STRING([--disable-gui], [Disable the Gtk graphical interface]),
			  [enable_gui=$enableval],
			  [enable_gui="yes"])
if test "$enable_gui" = "yes"; then
  AC_DEFINE([WITH_GUI], [1], [Enable Gtk graphical interface])
  PKG_CHECK_MODULES([GTK], [gtk+-2.0])
else
  PKG_CHECK_MODULES([GIO], [gio-2.0])
fi
AM_CONDITIONAL([WITH_GUI], [test "$enable_gui" = "yes"])

AC_SUBST([WITH_MPDCLIENT])
AC_ARG_ENABLE([mpdclient],
              AS_HELP_STRING([--with-mpdclient], [Enable mpd  client(default is YES)]),
              [enable_mpdclient=$enableval],
              [enable_mpdclient="yes"])
if test "$enable_mpdclient" = "yes"; then
  enable_dbus=yes
  AC_DEFINE([WITH_MPDCLIENT], [1], [Enable mpd music player])
  PKG_CHECK_MODULES([MPDCLIENT], [libmpdclient])
fi
AM_CONDITIONAL([WITH_MPDCLIENT], [test "$enable_mpdclient" = "yes"])

AC_SUBST([WITH_AUDCLIENT])
AC_ARG_ENABLE([audclient],
              AS_HELP_STRING([--disable-audclient], [Disable Audacious client (enabled by default)]),
              [enable_audclient=$enableval],
              [enable_audclient="yes"])
if test "$enable_audclient" = "yes"; then
  enable_dbus=yes
  AC_DEFINE([WITH_AUDCLIENT], [1], [Enable Audacious music player])
  PKG_CHECK_MODULES([AUDACIOUS], [audacious])
fi
AM_CONDITIONAL([WITH_AUDCLIENT], [test "$enable_audclient" = "yes"])

# Enable hardened compile and link flags
AC_ARG_ENABLE([harden_flags],
  [AS_HELP_STRING([--disable-harden-flags], [disable hardened compilier and linker flags])],
  [enable_harden_flags=$enableval],
  [enable_harden_flags="yes"])

# Check that harden CFLAGS will compile
AS_IF([test "$enable_harden_flags" = "yes"],
  HARDEN_CFLAGS="-fstack-protector --param=ssp-buffer-size=4 -Wformat -Wformat-security -Werror=format-security"
  [ AC_MSG_CHECKING([compiler supports harden flags])
  save_harden_cflags="$CFLAGS"
  CFLAGS="$CFLAGS $HARDEN_CFLAGS"
  AC_COMPILE_IFELSE([AC_LANG_PROGRAM(,,)],
      [AC_MSG_RESULT([yes])],
     [AC_MSG_RESULT([no]); HARDEN_CFLAGS='']
    )
  CFLAGS="$save_harden_cflags"],
  [HARDEN_CFLAGS=""])
AC_SUBST([HARDEN_CFLAGS])

dnl Check for language stuff
AM_GNU_GETTEXT([external])
AM_GNU_GETTEXT_VERSION([0.19.6])

# Checks for libraries.

if test "$enable_dbus" = "yes"; then
  AC_DEFINE([WITH_DBUSGLIB], [1], [Build with dbus-glib])
  PKG_CHECK_MODULES([DBUS_GLIB], [dbus-glib-1])
fi

AC_ARG_ENABLE([latin1],
              AS_HELP_STRING([--disable-latin1],
                             [Disable handling of tags encoded in Latin-1, and assume tags are UTF-8 encoded.]),
              [assume_latin1=$enableval],
              [assume_latin1="yes"])
if test "$assume_latin1" = "yes"; then
  AC_DEFINE([ASSUME_LATIN1], [1], [Assume tag-strings are encoded in Latin-1])
fi

PKG_CHECK_MODULES([GSL], [gsl])

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
