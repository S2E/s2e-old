BINARIES_I386 = demos/quicksort init_env/init_env.so s2ecmd/s2ecmd s2eget/s2eget
BINARIES_AMD64 = demos/quicksort init_env/init_env.so init_env/init_env64.so  s2ecmd/s2ecmd s2eget/s2eget
BINARIES_ARM = demos/quicksort init_env/init_env.so  s2ecmd/s2ecmd s2eget/s2eget
CFLAGS   = -Iinclude -Wall -g -O0 -std=c99
LDLIBS   = -ldl

i386: $(BINARIES_I386)
amd64: $(BINARIES_AMD64)
arm: $(BINARIES_ARM)

all: i386

s2ecmd/s2ecmd s2eget/s2eget: include/s2e.h

%: %.c
	$(CC) $(CCFLAGS) $(CFLAGS) $< -o $@

init_env/init_env.so: init_env/init_env.c
	$(CC) $(CCFLAGS) -fPIC -shared $(CFLAGS) $^ -o $@ $(LDLIBS)

clean:
	rm -f $(BINARIES)

install: $(BINARIES)
	cp $(BINARIES) $(BUILD_DIR)

.PHONY: all clean install i386 amd64 arm
