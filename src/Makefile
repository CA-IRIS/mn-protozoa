CC = gcc
CFLAGS = -Os -Wall -combine -fwhole-program
#CFLAGS = -Wall -ggdb
TARGET = protozoa

all:  $(TARGET)

SOURCES = poller.c channel.c config.c ccpacket.c buffer.c axis.c joystick.c manchester.c vicon.c pelco_d.c ccreader.c ccwriter.c log.c

$(TARGET): main.c $(SOURCES)
	$(CC) -o $(TARGET) $(CFLAGS) $(SOURCES) $<

clean:
	rm -f *.o $(TARGET)
