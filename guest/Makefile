BINARIES = demos/quicksort init_env/init_env.so s2ecmd/s2ecmd s2eget/s2eget
CFLAGS   = -Iinclude -Wall -m32
LDLIBS   = -ldl

all: $(BINARIES)

demos/quicksort init_env/init_env.so: CFLAGS+=-g -O0 -std=c99
s2ecmd/s2ecmd s2eget/s2eget: include/s2e.h

%: %.c
	$(CC) $(CFLAGS) $< -o $@

%.so: %.c
	$(CC) $(CFLAGS) -fPIC -shared $^ -o $@ $(LDLIBS)

clean:
	rm -f $(BINARIES)

install: $(BINARIES)
	cp $(BINARIES) $(BUILD_DIR)

.PHONY: all clean install
