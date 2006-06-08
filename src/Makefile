CC = gcc
CFLAGS = -O2 -Wall
STEST = protozoa

all:  $(STEST)

$(STEST): poller.c sport.c ccpacket.c buffer.c manchester.c
	$(CC) -o $(STEST) $(CFLAGS) sport.c ccpacket.c buffer.c manchester.c $<
