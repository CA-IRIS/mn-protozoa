CC = gcc
CFLAGS = -Os -Wall -combine -fwhole-program
STEST = protozoa

all:  $(STEST)

$(STEST): poller.c sport.c ccpacket.c buffer.c manchester.c vicon.c
	$(CC) -o $(STEST) $(CFLAGS) sport.c ccpacket.c buffer.c manchester.c vicon.c $<
