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

## Things to compare with

Big engines
* SpiderMonkey's interpreter mode
* ChakraCore's interpreter mode
* V8's interpreter mode

Smaller interpreters
* duktape
* jerryscript

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
* string, symbol, object (pointer to obj struct)
* null, undefined (special values)
* boolean (special values)

Many VMs do crazy things like using NaN bits in doubles to signal that the
64-bit value actually contains an int32_t or pointer instead of a double.
* todo -- check the NaN rules for WebAssembly if that's something that's safe!


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

Because JS is garbage-collected, we need to use a GC engine of some kind
that can link into C/C++.

The traditional Boehm libgc seems like it won't work here since it can't
access the wasm/js stack directly.

I'm trying my own naive mark-and-sweep GC based on maintaining my own
stack of scope objects.

Each scope contains a pointer to its parent scope, a vector of JS values
that were passed in as arguments or allocated for locals, and a vector of
pointers to JS values captured from outer scopes.

The actual 'local variables' used in the compiled functions will be pointers,
allowing them to be modified in-place in the stack frames by closures, which
should keep the correct behavior.

The marking phase starts with all our roots (global object and the stack)
TODO: stack!


## Closures

Oh man this is more complex than I thought if don't want to leak memory.

Ok there's several classes of variable bindings in JS:

* uncaptured local args/vars
  * allocate in the frame, kept alive by stack push/pop
* names captured from an outside scope
  * get a pointer to the actual value from the scope
* names that are captured by any lower scope
  * allocate in a scope, which is kept alive by the function definition
  * referencing higher scopes?
    * copy the used pointers into the local array
    * scope chain to retain GC
  * what about... capturing an argument that's modified?
    * could rewrite that into copying the arg into the capture state.
    * have the _Scope_ carry vals
    * have the _Capture_ carry pointers, which can reach arbitrarily high
    * the Capture references the current scope. Or a list of all scopes it uses?

```js

var a = "a";

function foo() {
  // we actually capture a from global
  // so we can pass it in to bar's creation

  var b = "b";

  function bar() {
    // a is captured from global
    // b is captured from foo
    a += b;
  }

  bar();
}
foo();
console.log(a); // "ab"

```


# Naive implementation

Let's assume the runtime is implemented in C++. Naive implementation for totally
polymorphic code looks something like:
* [aotjs_runtime.h](aotjs_runtime.h)
* [aotjs_runtime.cpp](aotjs_runtime.cpp)

Example hand-compiled programs using it:
* [samples/closure.js](samples/closure.js) -> [samples/closure.cpp](samples/closure.cpp)
