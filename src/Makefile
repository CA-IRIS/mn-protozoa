CC = gcc
CFLAGS = -O2 -Wall
#CFLAGS = -Wall -ggdb
TARGET = protozoa

all:  $(TARGET)

SOURCES = poller.c channel.c config.c ccpacket.c buffer.c axis.c joystick.c manchester.c vicon.c pelco_d.c pelco_p.c infinova.c ccreader.c ccwriter.c log.c pool.c rbtree.c stats.c timer.c defer.c timeval.c

$(TARGET): main.c $(SOURCES)
	$(CC) -o $(TARGET) $(CFLAGS) $(SOURCES) $<

clean:
	rm -f *.o $(TARGET)
