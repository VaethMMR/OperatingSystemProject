CC=gcc
INCLUDE=./Structures/
CFLAGS=-Wall -I$(INCLUDE)
OBJS=rlmain.o rlist.o queue.o

.PHONY: all clean 

all: rlmain

queue.o: $(INCLUDE)queue.c $(INCLUDE)queue.h
	$(CC) $(CFLAGS) -c $(INCLUDE)queue.c -o queue.o

rlist.o: $(INCLUDE)rlist.c $(INCLUDE)rlist.h queue.o
	$(CC) $(CFLAGS) -c $(INCLUDE)rlist.c -o rlist.o

rlmain: main.c queue.o rlist.o
	$(CC) $(CFLAGS) main.c rlist.o queue.o -o main

clean: 
	rm -f *.o rlmain
