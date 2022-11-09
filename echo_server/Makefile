CC = gcc
TARGET = server
OBJS = echoserveri.o csapp.o
CFLAGS = -Wall -g
LDFLAGS = -lpthread

all : $(TARGET)

$(TARGET) : $(OBJS)
	$(CC) -o $(TARGET) $(OBJS) $(LDFLAGS)

csapp.o : csapp.c csapp.h
	$(CC) -c csapp.c

echoserveri.o : echoserveri.c
	$(CC) -c echoserveri.c

clean:
	rm -f *.o $(TARGET)