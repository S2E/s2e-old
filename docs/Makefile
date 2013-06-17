RST_FILES := $(wildcard *.rst */*.rst)
HTML_FILES := $(RST_FILES:.rst=.html)

all: $(HTML_FILES)

clean:
	rm -f $(HTML_FILES)

%.html: %.rst
	./rst2html-pygments \
	    --stylesheet=$$(echo $< | sed -e 's:[^/]\+/:../:g' | xargs dirname)/s2e.css \
	    --link-stylesheet --source-link --no-toc-backlinks \
	    --input-encoding utf-8:strict $< $@
