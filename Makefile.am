
SUBDIRS = po doc icons

bin_PROGRAMS = gjay

gjay_LDADD = @GTK_LIBS@ @DBUS_GLIB_LIBS@ @GSL_LIBS@
AM_CFLAGS = -Wall @GTK_CFLAGS@ @DBUS_GLIB_CFLAGS@ @GSL_CFLAGS@

gjay_SOURCES = gjay.h songs.h prefs.h ui.h rgbhsv.h analysis.h playlist.h \
							 ipc.h constants.h vorbis.h mp3.h flac.h i18n.h \
							 dbus.h util.h \
							 play_audacious.h \
							 gjay.c dbus.c ipc.c prefs.c songs.c rgbhsv.c ui.c \
							 ui_explore_view.c ui_prefs_view.c ui_selection_view.c \
							 ui_playlist_view.c ui_colorwheel.c \
							 ui_menubar.c analysis.c playlist.c \
							 vorbis.c mp3.c flac.c util.c \
							 play_common.c play_common.h \
							 play_audacious.c plaY_audacious.h \
							 play_mpdclient.c play_mpdclient.h
#play_exaile.h play_exaile.c

#ddui_colorbox.c 

ACLOCAL_AMFLAGS = -I m4

EXTRA_DIST = config.rpath m4/ChangeLog
