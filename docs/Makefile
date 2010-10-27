RST_FILES := $(shell find . -name "*.rst")
HTML_FILES := $(RST_FILES:.rst=.html)

all: $(HTML_FILES)

clean:
	rm -f $(HTML_FILES)

%.html: %.rst
	./rst2html-pygments --stylesheet=s2e.css \
	    --link-stylesheet --source-link --no-toc-backlinks \
	    --input-encoding utf-8:strict $< $@
