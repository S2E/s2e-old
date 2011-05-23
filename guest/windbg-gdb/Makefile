PROGRAM=gdb.dll
SOURCES=BFDInterface.cpp main.cpp Symbols.cpp
WINDBG=/c/Program Files/Debugging Tools for Windows (x64)
WINDBG_INC=$(WINDBG)/sdk/inc
WINDBG_LIB=$(WINDBG)/sdk/lib

%.o:%.cpp
	g++ -O3 -m64 -I"$(WINDBG_INC)" -c -o $@ $<

$(PROGRAM): $(SOURCES:.cpp=.o)
	g++ $^ -m64 -shared -o $(PROGRAM)  -ldbgeng -lbfd -lintl -liberty -lz -lgettextpo -lstdc++ -lpsapi
	#gcc -x c++ -E -Wall -shared -o $(PROGRAM).i $(SOURCES)

	#-I"$(WINDBG_INC)"
	
clean:
	rm -f $(PROGRAM) *.o