aotjs -- ahead-of-time compilation of JavaScript to LLVM

# Intro

At this stage this is a thought experiment.

## Intent

Safe, small, moderate-performance JS runtime for application extensions.

## Goals

* static compilation of JavaScript to LLVM bitcode
    * native and WebAssembly-based execution
* memory safety & sandboxing
    * JS code cannot affect things not exposed to it
* small code size
* better performance than an interpreter
* small, but standard-ish JS language implementation
    * target ES6 or so for starters
* ability to call to the JS runtime from linked C
* ability to add JS objects from linked C

## Non-goals

* not expecting optimized code for polymorphic stuff
* no support for loading new code at runtime
* no support for runtime eval() or new Function("source")
* no in-process resource limits for memory or execution time


# Prior art

* found a random old project https://github.com/eddid/jslang
* pretty out of date, but might have cool ideas?

# Things to compare with

* SpiderMonkey's interpreter mode
* ChakraCore's interpreter mode
* V8's interpreter mode

# How it might work

* direct from LLVM to wasm
    * hello.js -> hello.o -> hello.wasm
* emit C source code
    * hello.js -> hello.c -> hello.o -> hello.wasm


# Language considerations

## Types

JS values may be of these types:
* number (double)
* number (int32_t optimized subset of doubles)
* string, object (pointer to obj struct)
* null, undefined (special values)
* boolean (special values)

Many VMs do crazy things like using NaN bits in doubles to signal that the
64-bit value actually contains an int32_t or pointer instead of a double.
* todo -- check the NaN rules for WebAssembly if that's something that's safe!

## Garbage collection
Because JS is garbage-collected, we need to use a GC engine of some kind
that can link into C.
* todo -- check out boehmgc and such

## Exceptions

WebAssembly participates in the host JavaScript environment's stack unwinding
when exceptions are thrown, but provides no way to catch an exception or get
information about a stack trace. Ouch!

This means we either have to avoid doing exception handling, or every function
call has to be followed by a check for exception state. emscripten does this
optionally for C++ code but it's known to be slow, so it's generally recommended
to disable exceptions (!).

Hypothetically you could also build both fast-path and slow-path versions of
all functions, so the slow-path gets called if inside a `try` block. This would
increase binary size, though, and is probably not worth it for my use case
where safety and download size are more likely important than runtime speed.


## Garbage collection

Note that the well-known Boehm GC library needs access to stack and may not work?

Possibility:
* start with a GC root (world)
* maintain a list/refcount of in-scope objects via constructors/destructors of referencess
* at GC time


# Naive implementation thoughts

Let's assume the runtime is implemented in C++. Naive implementation for totally
polymorphic code looks something like [aotjs_runtime.cpp](aotjs_runtime.cpp)




# Other interpreters

* duktape
* jerryscript
