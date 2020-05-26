CC = gcc
RM = rm -f

default: all

all: server

server: webserver.c
	$(CC) -o webserver webserver.c -lm

clean:
	$(RM) webserver