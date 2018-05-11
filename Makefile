SOURCES=aotjs_runtime.cpp
HEADERS=aotjs_runtime.h
CFLAGS=-g -O0 -std=c++14
CFLAGS_NATIVE=$(CFLAGS)
CFLAGS_WASM=$(CFLAGS) -s WASM=1 -s BINARYEN_TRAP_MODE=clamp

all : native js

native : aotjs closure

js : aotjs.js closure.js

clean :
	rm -f aotjs
	rm -f aotjs.html
	rm -f aotjs.js
	rm -f aotjs.wasm
	rm -f aotjs.wasm.map
	rm -f aotjs.wast
	rm -rf aotjs.dSYM
	rm -f closure
	rm -f closure.html
	rm -f closure.js
	rm -f closure.wasm
	rm -f closure.wasm.map
	rm -f closure.wast
	rm -rf closure.dSYM

aotjs : aotjs.cpp $(SOURCES) $(HEADERS)
	clang++ $(CFLAGS_NATIVE) -o aotjs aotjs.cpp $(SOURCES)

closure : samples/closure.cpp $(SOURCES) $(HEADERS)
	clang++ $(CFLAGS_NATIVE) -o closure samples/closure.cpp $(SOURCES)

aotjs.js : $(SOURCES) $(HEADERS)
	em++ $(CFLAGS_WASM) -o aotjs.html aotjs.cpp $(SOURCES)

closure.js : samples/closure.cpp $(SOURCES) $(HEADERS)
	em++ $(CFLAGS_WASM) -o closure.html aotjs.cpp $(SOURCES)
