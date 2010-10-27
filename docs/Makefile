RST_FILES := $(shell find . -name "*.rst")
HTML_FILES := $(RST_FILES:.rst=.html)

ifeq ($(shell uname),Darwin)
SED := gsed
else
SED := sed
endif

all: $(HTML_FILES)

clean:
	rm -f $(HTML_FILES)

%.html: %.rst
	./rst2html-pygments \
	    --stylesheet=$$(echo $< | $(SED) 's:[^/]\+/:../:g' | xargs dirname)/s2e.css \
	    --link-stylesheet --source-link --no-toc-backlinks \
	    --input-encoding utf-8:strict $< $@
