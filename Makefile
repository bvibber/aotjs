SOURCES=aotjs_runtime.cpp
HEADERS=aotjs_runtime.h
#CFLAGS=-g -O3 -std=c++14
CFLAGS=-g -O3 -std=c++14 -DFORCE_GC
#CFLAGS=-g -O0 -std=c++14 -DFORCE_GC -DDEBUG
#CFLAGS=-g -O0 -std=c++14
CFLAGS_NATIVE=$(CFLAGS) -fstandalone-debug
CFLAGS_WASM=$(CFLAGS) -s WASM=1 -s BINARYEN_TRAP_MODE=clamp -s NO_FILESYSTEM=1

all : native wasm

native : gc closure retval

wasm : gc.js closure.js retval.js

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
	rm -f retval
	rm -f retval.html
	rm -f retval.js
	rm -f retval.wasm
	rm -f retval.wasm.map
	rm -f retval.wast
	rm -rf retval.dSYM

gc : samples/gc.cpp $(SOURCES) $(HEADERS)
	clang++ $(CFLAGS_NATIVE) -o gc samples/gc.cpp $(SOURCES)

closure : samples/closure.cpp $(SOURCES) $(HEADERS)
	clang++ $(CFLAGS_NATIVE) -o closure samples/closure.cpp $(SOURCES)

retval : samples/retval.cpp $(SOURCES) $(HEADERS)
	clang++ $(CFLAGS_NATIVE) -o retval samples/retval.cpp $(SOURCES)

gc.js : samples/gc.cpp $(SOURCES) $(HEADERS)
	em++ $(CFLAGS_WASM) -o gc.js samples/gc.cpp $(SOURCES)

closure.js : samples/closure.cpp $(SOURCES) $(HEADERS)
	em++ $(CFLAGS_WASM) -o closure.js samples/closure.cpp $(SOURCES)

retval.js : samples/retval.cpp $(SOURCES) $(HEADERS)
	em++ $(CFLAGS_WASM) -o retval.js samples/retval.cpp $(SOURCES)
