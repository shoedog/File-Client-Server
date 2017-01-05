CC=g++
CFLAGS= -g -Wall

all: ftserver

ftserver:
	$(CC) $(CFLAGS) ftserver.cpp -o ftserver

clean: 
	$(RM) $(TARGET) *.o ftserver
