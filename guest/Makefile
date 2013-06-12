BINARIES = demos/quicksort init_env/init_env.so s2ecmd/s2ecmd s2eget/s2eget
CFLAGS   = -Iinclude -Wall -g -O0 -std=c99

all: $(BINARIES)

s2ecmd/s2ecmd s2eget/s2eget: include/s2e.h

%: %.c
	$(CC) $(CFLAGS) $< -o $@

init_env/init_env.so: init_env/init_env.c
	$(CC) $(CFLAGS) -fPIC -shared $^ -o $@ -ldl

clean:
	rm -f $(BINARIES)

install: $(BINARIES)
	cp $(BINARIES) $(BUILD_DIR)

.PHONY: all clean install
