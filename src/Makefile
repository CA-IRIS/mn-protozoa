CC = gcc
CFLAGS = -Os -Wall -combine -fwhole-program
#CFLAGS = -Wall -ggdb
STEST = protozoa

all:  $(STEST)

$(STEST): main.c poller.c config.c sport.c ccpacket.c buffer.c manchester.c vicon.c combiner.c
	$(CC) -o $(STEST) $(CFLAGS) poller.c config.c sport.c ccpacket.c buffer.c manchester.c vicon.c combiner.c $<
