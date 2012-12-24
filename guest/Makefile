DIRECTORIES:=init_env s2ecmd s2eget demos

all:
	for dir in $(DIRECTORIES); \
	do \
	    cd $$dir; make; cd ..; \
	done

.PHONY: .clean
clean:
	for dir in $(DIRECTORIES); \
	do \
	    cd $$dir; make clean; cd ..; \
	done

install:
	for dir in $(DIRECTORIES); \
	do \
		cd $$dir; make install; cd ..; \
	done
