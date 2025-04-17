CC      = gcc
CFLAGS  = -Wall -g
OBJS    = disk.o fs.o main.o

all: demo

disk.o: disk.c disk.h
	$(CC) $(CFLAGS) -c disk.c

fs.o: fs.c fs.h disk.h
	$(CC) $(CFLAGS) -c fs.c

main.o: main.c fs.h
	$(CC) $(CFLAGS) -c main.c

demo: $(OBJS)
	$(CC) $(CFLAGS) -o demo $(OBJS)

clean:
	rm -f *.o demo disk.fs