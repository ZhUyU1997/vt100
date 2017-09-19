CFLAGS=-std=c99 -Wall -Wextra 
CC=gcc
LDFLAGS=-lGL -lglut -lm
TARGET=vt100
.PHONY: all clean

all: ${TARGET}

clean:
	rm -fv ${TARGET} *.o
