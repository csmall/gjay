
SUBDIRS = doc icons

bin_PROGRAMS = gjay

LDADD = @GTK_LIBS@ @DBUS_GLIB_LIBS@ @GSL_LIBS@
AM_CFLAGS = @GTK_CFLAGS@ @DBUS_GLIB_CFLAGS@ @GSL_CFLAGS@

gjay_SOURCES = gjay.h songs.h prefs.h ui.h rgbhsv.h analysis.h playlist.h \
							 ipc.h constants.h vorbis.h mp3.h \
							 gjay.c dbus.c ipc.c prefs.c songs.c rgbhsv.c ui.c \
							 ui_explore_view.c ui_prefs_view.c ui_selection_view.c \
							 ui_playlist_view.c ui_colorwheel.c ui_colorbox.c \
							 ui_menubar.c analysis.c playlist.c gjay_audacious.c \
							 vorbis.c mp3.c

