SOURCES=aotjs_runtime.cpp
HEADERS=aotjs_runtime.h
#CFLAGS=-g -O0 -std=c++14 -DDEBUG
CFLAGS=-O2 -std=c++14
CFLAGS_NATIVE=$(CFLAGS)
CFLAGS_WASM=$(CFLAGS) -s WASM=1 -s BINARYEN_TRAP_MODE=clamp -s NO_FILESYSTEM=1

all : native wasm

native : gc closure

wasm : gc.html closure.html

clean :
	rm -f gc
	rm -f gc.html
	rm -f gc.js
	rm -f gc.wasm
	rm -f gc.wasm.map
	rm -f gc.wast
	rm -rf gc.dSYM
	rm -f closure
	rm -f closure.html
	rm -f closure.js
	rm -f closure.wasm
	rm -f closure.wasm.map
	rm -f closure.wast
	rm -rf closure.dSYM

gc : samples/gc.cpp $(SOURCES) $(HEADERS)
	clang++ $(CFLAGS_NATIVE) -o gc samples/gc.cpp $(SOURCES)

closure : samples/closure.cpp $(SOURCES) $(HEADERS)
	clang++ $(CFLAGS_NATIVE) -o closure samples/closure.cpp $(SOURCES)

gc.html : samples/gc.cpp $(SOURCES) $(HEADERS)
	em++ $(CFLAGS_WASM) -o gc.html samples/gc.cpp $(SOURCES)

closure.html : samples/closure.cpp $(SOURCES) $(HEADERS)
	em++ $(CFLAGS_WASM) -o closure.html samples/closure.cpp $(SOURCES)
