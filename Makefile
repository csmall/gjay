CC := gcc
LINK := $(CC)
CFLAGS := -d -g -Wall `gtk-config --cflags`
LFLAGS := `gtk-config --libs` -lxmms -lgsl -lgslcblas -lm -lpthread -lvorbis -lvorbisfile
TARGET := gjay
HEADERS = \
	gjay.h \
	rgbhsv.h \
	analysis.h 
OBJECTS = \
	gjay.o \
	prefs.o \
	songs.o \
	rgbhsv.o \
	ui.o \
	ui_components.o \
	ui_color_wheel.o \
	ui_star_rating.o \
	ui_categorize.o \
	gjay_xmms.o \
	spectrum.o \
	analysis.o \
	bpm.o \
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
