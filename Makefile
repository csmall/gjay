CC := gcc
LINK := $(CC)
CFLAGS := -d -g -Wall `gtk-config --cflags`
LFLAGS := `gtk-config --libs` -lxmms -lgsl -lgslcblas -lm -lpthread -lvorbis -lvorbisfile
TARGET := gjay
HEADERS = \
	gjay.h \
	rgbhsv.h \
	analysis.h 
SOURCES = \
	gjay.c \
	prefs.c \
	songs.c \
	rgbhsv.c \
	ui.c \
	ui_components.c \
	ui_color_wheel.c \
	ui_star_rating.c \
	ui_categorize.c \
	gjay_xmms.c \
	analysis.c \
	spectrum.c \
	bpm.c \
	playlist.c
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


all: $(TARGET)

.SUFFIXES: .c
.c.o: $(HEADERS)
	$(CC) -c $(CFLAGS) -o $@ $<


$(TARGET): $(OBJECTS) 
	$(LINK) $(LFLAGS) -o $(TARGET) $(OBJECTS)

clean:
	-rm -f *.a *.o *~ data/*~ core $(TARGET) 
