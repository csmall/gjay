CC := gcc
LINK := $(CC)
CFLAGS := -d -g -Wall `pkg-config --cflags gtk+-2.0`
LFLAGS := `pkg-config --libs gtk+-2.0` -lxmms -lgsl -lgslcblas -lm -lpthread -lvorbis -lvorbisfile
TARGET := gjay
HEADERS = \
	gjay.h \
	songs.h \
	prefs.h \
	ui.h \
	rgbhsv.h \
	analysis.h \
	playlist.h
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

INSTALL=/usr/bin/install -o root -g root -m 755

all: $(TARGET) 

%.o: %.c $(HEADERS)
	$(CC) -c $(CFLAGS) -o $@ $<


$(TARGET): $(OBJECTS)  
	$(LINK) $(LFLAGS) -o $(TARGET) $(OBJECTS)

install: $(TARGET)
	$(INSTALL) gjay $(DESTDIR)/usr/bin/gjay

clean:
	-rm -f *.a *.o *~ data/*~ core $(TARGET) 
