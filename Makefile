CC = gcc
CFLAGS = -Wall -fsanitize=address -std=c99 -O2 -g

all: mysh

mysh: mysh.c
	$(CC) -o $@ $<
#$(CFLAGS)

clean:
	rm -f mysh
