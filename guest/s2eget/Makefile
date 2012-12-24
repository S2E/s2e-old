BINARIES:=s2eget

all: $(BINARIES)

s2eget: s2eget.c ../include/s2e.h
	gcc -I../include -Wall -m32 s2eget.c -o s2eget

clean:
	rm -f $(BINARIES)

install: $(BINARIES)
	cp $(BINARIES) $(BUILD_DIR)
