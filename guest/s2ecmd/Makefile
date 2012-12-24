BINARIES:=s2ecmd

all: $(BINARIES)

s2ecmd: s2ecmd.c ../include/s2e.h
	$(CC) -I../include -Wall -m32 s2ecmd.c -o $@

clean:
	rm -f $(BINARIES)

install: $(BINARIES)
	cp $(BINARIES) $(BUILD_DIR)
