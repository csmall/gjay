
SUBDIRS = po doc icons

bin_PROGRAMS = gjay

gjay_LDADD = $(GIO_LIBS) $(GTK_LIBS) $(DBUS_GLIB_LIBS) $(GSL_LIBS)
AM_CFLAGS = -Wall $(GIO_CFLAGS) $(GTK_CFLAGS) $(DBUS_GLIB_CFLAGS) $(GSL_CFLAGS) $(HARDEN_CFLAGS)

GUI_EXTRA = ui.h ui.c ui_menubar.c ui_private.h\
	      ui_prefs_view.c \
	      ui_selection.c ui_selection.h \
	      ui_playlist_view.c ui_colors.c ui_explore.c ui_explore.h
extra_DIST = 
gjay_SOURCES = gjay.h songs.h prefs.h colors.h analysis.h playlist.h \
							 ipc.h constants.h vorbis.h mp3.h flac.h i18n.h \
							 dbus.h util.h \
							 gjay.c dbus.c ipc.c prefs.c songs.c colors.c \
							 analysis.c playlist.c \
							 vorbis.c mp3.c flac.c util.c \
							 play_common.c play_common.h 
#play_exaile.h play_exaile.c

if WITH_GUI
  gjay_SOURCES += $(GUI_EXTRA)
else
  extra_DIST += $(GUI_EXTRA)
endif

if WITH_AUDCLIENT
gjay_SOURCES += play_audacious.c play_audacious.h
else
  extra_DIST += play_audacious.c play_audacious.h
endif

if WITH_MPDCLIENT
gjay_SOURCES += play_mpdclient.c play_mpdclient.h
else
  extra_DIST += play_mpdclient.c play_mpdclient.h
endif
#ddui_colorbox.c 

ACLOCAL_AMFLAGS = -I m4

EXTRA_DIST = config.rpath m4/ChangeLog autogen.sh
