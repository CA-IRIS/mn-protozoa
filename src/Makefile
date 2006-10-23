CC = gcc
CFLAGS = -Os -Wall -combine -fwhole-program
#CFLAGS = -Wall -ggdb
#CFLAGS = -Os -Wall -S -combine
TARGET = protozoa

all:  $(TARGET)

$(TARGET): main.c poller.c channel.c config.c sport.c tcp.c ccpacket.c buffer.c manchester.c vicon.c pelco_d.c ccreader.c ccwriter.c
	$(CC) -o $(TARGET) $(CFLAGS) poller.c channel.c config.c sport.c tcp.c ccpacket.c buffer.c manchester.c vicon.c pelco_d.c ccreader.c ccwriter.c $<
