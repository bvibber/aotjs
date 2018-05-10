SOURCES=aotjs_runtime.cpp
HEADERS=aotjs_runtime.h
CFLAGS=-g -std=c++14
#CFLAGS=-O2 -std=c++14

all : native closure

native : aotjs

js : aotjs.js

clean :
	rm -f aotjs
	rm -f aotjs.js
	rm -f aotjs.wasm
	rm -f aotjs.wasm.map
	rm -f aotjs.wast
	rm -rf aotjs.dSYM
	rm -f closure

aotjs : aotjs.cpp $(SOURCES) $(HEADERS)
	clang++ $(CFLAGS) -o aotjs aotjs.cpp $(SOURCES)

closure : samples/closure.cpp $(SOURCES) $(HEADERS)
	clang++ $(CFLAGS) -o closure samples/closure.cpp $(SOURCES)

aotjs.js : $(SOURCES) $(HEADERS)
	em++ $(CFLAGS) -o aotjs.js -s WASM=1 $(SOURCES)
