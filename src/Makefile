CC = gcc
CFLAGS = -Os -Wall -combine -fwhole-program
#CFLAGS = -Wall -ggdb
TARGET = protozoa

all:  $(TARGET)

$(TARGET): main.c poller.c config.c sport.c ccpacket.c buffer.c manchester.c vicon.c combiner.c
	$(CC) -o $(TARGET) $(CFLAGS) poller.c config.c sport.c ccpacket.c buffer.c manchester.c vicon.c combiner.c $<
