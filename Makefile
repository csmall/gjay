PREFIX = /usr/local
CC = gcc
LINK = $(CC)
CFLAGS = -g -Wall `pkg-config --cflags gtk+-2.0`
LFLAGS = `pkg-config --libs gtk+-2.0` -lxmms -lgsl -lgslcblas -lm -lpthread -lvorbis -lvorbisfile
TARGET = gjay

HEADERS = \
	gjay.h \
	songs.h \
	prefs.h \
	ui.h \
	rgbhsv.h \
	analysis.h \
	playlist.h \
	ipc.h
OBJECTS = \
	gjay.o \
	ipc.o \
	prefs.o \
	songs.o \
	rgbhsv.o \
	ui.o \
	ui_about_view.o \
	ui_explore_view.o \
	ui_prefs_view.o \
	ui_selection_view.o \
	ui_playlist_view.o \
	ui_colorwheel.o \
	gjay_xmms.o \
	analysis.o \
	playlist.o

INSTALL = /usr/bin/install -o root -g root -m

all: $(TARGET) 

%.o: %.c $(HEADERS)
	$(CC) -c $(CFLAGS) -o $@ $<


$(TARGET): $(OBJECTS)  
	$(LINK) $(LFLAGS) -o $(TARGET) $(OBJECTS)

doc: doc/gjay.1.gz

doc/gjay.1.gz: doc/gjay.1
	gzip -9 < doc/gjay.1 > doc/gjay.1.gz

install: $(TARGET) doc
	$(INSTALL) 755    gjay $(PREFIX)/bin/gjay 
	$(INSTALL) 644    icons/* $(PREFIX)/share/gjay/icons/
	$(INSTALL) 755 -d $(PREFIX)/share/man/man1
	$(INSTALL) 644    doc/gjay.1.gz $(PREFIX)/share/man/man1/gjay.1.gz

clean:
	-rm -f *.a *.o *~ data/*~ core $(TARGET) doc/*.gz

