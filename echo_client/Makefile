CC = gcc
TARGET = client
OBJS = echoclient.o csapp.o
CFLAGS = -Wall -g
LDFLAGS = -lpthread

all : $(TARGET)

$(TARGET) : $(OBJS)
	$(CC) -o $(TARGET) $(OBJS) $(LDFLAGS)

csapp.o : csapp.c csapp.h
	$(CC) -c csapp.c

echoclient.o : echoclient.c csapp.h
	$(CC) -c echoclient.c

clean:
	rm -f *.o $(TARGET)