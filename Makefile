SOURCES=aotjs_runtime.cpp aotjs.cpp
HEADERS=aotjs_runtime.h
#CFLAGS=-g
CFLAGS=-O2

all : native

native : aotjs

js : aotjs.js

clean :
	rm -f aotjs
	rm -f aotjs.js
	rm -f aotjs.wasm

aotjs : $(SOURCES) $(HEADERS)
	clang++ $(CFLAGS) -std=c++14 -o aotjs $(SOURCES)

aotjs.js : $(SOURCES) $(HEADERS)
	em++ $(CFLAGS) -std=c++14 -o aotjs.js -s WASM=1 $(SOURCES)
