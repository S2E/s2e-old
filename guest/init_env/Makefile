CFLAGS=-m32 -Wall -g -O0 -I../include -std=c99
CXXFLAGS=-m32 -Wall -g -O0 -I../include
LDLIBS=-ldl
CC=gcc
CXX=g++

BINARIES=init_env.so

%.so: %.c
	$(CC) $(CFLAGS) -fPIC -shared $(LDLIBS) -o $@ $^

.PHONY: all
all: $(BINARIES)

.PHONY: clean
clean:
	rm -f $(BINARIES)
