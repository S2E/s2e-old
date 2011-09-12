all: s2ecmd

s2ecmd: s2ecmd.c ../include/s2e.h
	$(CC) -I../include -Wall -m32 s2ecmd.c -o $@

clean:
	rm s2ecmd
