CC=clang++
#CC=g++
EMCC=em++

SOURCES=aotjs_runtime.cpp
HEADERS=aotjs_runtime.h
CFLAGS=-g -O3
#CFLAGS=-g -Os
#CFLAGS=-g -O3 -DFORCE_GC
#CFLAGS=-g -O0 -DFORCE_GC -DDEBUG
#CFLAGS=-g -O0

CFLAGS_COMMON=$(CFLAGS) -std=c++14
#CFLAGS_NATIVE=$(CFLAGS_COMMON) -flto
CFLAGS_NATIVE=$(CFLAGS_COMMON)
CFLAGS_WASM=$(CFLAGS_COMMON) -s WASM=1 -s BINARYEN_TRAP_MODE=clamp -s NO_FILESYSTEM=1 --llvm-lto 1

all : native wasm

native : build/gc build/closure build/retval build/args build/mandelbrot

wasm : build/gc.js build/closure.js build/retval.js build/args.js build/mandelbrot.js

clean :
	rm -rf build

build/%.js : samples/%.cpp $(SOURCES) $(HEADERS)
	mkdir -p build
	$(EMCC) $(CFLAGS_WASM) -o $@ $< $(SOURCES)
	gzip -9 < build/$*.wasm > build/$*.wasm.gz

build/% :: samples/%.cpp $(SOURCES) $(HEADERS)
	mkdir -p build
	$(CC) $(CFLAGS_NATIVE) -o $@ $< $(SOURCES)
