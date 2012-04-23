CFLAGS=-m32 -Wall -g -O0 -I../include -std=c99
CXXFLAGS=-m32 -Wall -g -O0 -I../include
CC=gcc
CXX=g++

BINARIES=quicksort

%.o: %.c
	$(CC) $(CFLAGS) -fPIC -o $@ $^

.PHONY: all
all: $(BINARIES)

.PHONY: clean
clean:
	rm -f $(BINARIES)
