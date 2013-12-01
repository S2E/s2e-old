include config.mak

BINARIES = init_env.so s2ecmd s2eget
CCFLAGS = -I$(TOOLS_DIR)/include -Wall -g -O0 -std=c99
LDLIBS = -ldl

all: $(BINARIES)

%: %.c
	$(CC) $(CCFLAGS) $(CFLAGS) $< -o $@

s2ecmd: $(TOOLS_DIR)/s2ecmd/s2ecmd.c $(TOOLS_DIR)/include/s2e.h
	$(CC) $(CCFLAGS) $(CFLAGS) $< -o $@

s2eget: $(TOOLS_DIR)/s2eget/s2eget.c $(TOOLS_DIR)/include/s2e.h
	$(CC) $(CCFLAGS) $(CFLAGS) $< -o $@

init_env.so: $(TOOLS_DIR)/init_env/init_env.c
	$(CC) $(CCFLAGS) -fPIC -shared $(CFLAGS) $^ -o $@ $(LDLIBS)

clean:
	rm -f $(BINARIES)

install: $(BINARIES)
	cp $(BINARIES) $(INSTALL_DIR)

.PHONY: all clean install

