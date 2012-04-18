all:
	for dir in init_env s2ecmd s2eget demos; \
	do \
	    cd $$dir; make; cd ..; \
	done

.PHONY: .clean
clean:
	for dir in init_env s2ecmd s2eget demos; \
	do \
	    cd $$dir; make clean; cd ..; \
	done
