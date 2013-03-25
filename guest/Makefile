BINARIES = demos/quicksort init_env/init_env.so init_env/init_env64.so  s2ecmd/s2ecmd s2eget/s2eget
CFLAGS   = -Iinclude -Wall -g -O0 -std=c99
LDLIBS   = -ldl

all: $(BINARIES)

demos/quicksort init_env/init_env.so s2ecmd/s2ecmd s2eget/s2eget : CFLAGS+=-m32
init_env/init_env64.so: CFLAGS+=-m64
s2ecmd/s2ecmd s2eget/s2eget: include/s2e.h

%: %.c
	$(CC) $(CFLAGS) $< -o $@

init_env/init_env.so init_env/init_env64.so: init_env/init_env.c
	$(CC) $(CFLAGS) -fPIC -shared $^ -o $@ $(LDLIBS)

clean:
	rm -f $(BINARIES)

install: $(BINARIES)
	cp $(BINARIES) $(BUILD_DIR)

.PHONY: all clean install
