aotjs -- ahead-of-time compilation of JavaScript to LLVM

# Intro

At this stage this is a thought experiment, with a very early proof of concept
of a garbage-collecting JS-like runtime written in C++, which works when
compiled to WebAssembly.

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
* ability to call to the JS runtime from host
* ability to add JS objects from host

## Non-goals

* not expecting optimized code for polymorphic stuff
* no support for loading new code at runtime
* no support for runtime eval() or new Function("source")
* no in-process resource limits for memory or execution time
    * WebAssembly sandbox can apply a hard memory limit
    * browser will eventually halt super-long loops

## Things to compare with

Big engines
* SpiderMonkey's interpreter mode
* ChakraCore's interpreter mode
* V8's interpreter mode

Smaller interpreters
* duktape
* jerryscript

Smaller compilers
* found a random old project https://github.com/eddid/jslang

# How it might work

* direct from LLVM to wasm
    * hello.js -> hello.o -> hello.wasm
* emit C++ source code
    * hello.js -> hello.cpp -> hello.o -> hello.wasm


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

Each scope contains a pointer to its parent scope (if it has captures),
a vector of JS values that were allocated for locals.

The actual 'local variables' used in the compiled functions will be pointers
(C++ references in the current PoC) allowing them to be modified in-place in
the stack frames by closures, which should keep the correct behavior.

The marking phase starts with all our roots (global object, the scope stack,
and the call frame stack) so any live objects get marked. Then a sweep goes
through the set of all objects (maintained manually for now, eventually should
be integrated with the allocator) and deletes any non-reachable objects.

Currently in the PoC, GC is either run manually, or if FORCE_GC is defined
then on every allocation to force debugging behavior.

The actual objects may do cleanup of their non-GC'd resources from a virtual
destructor, which gets called by the delete operation in the GC sweep.

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

Changed this to use a parallel stack of Val cells, moderated by smart pointers
in Local/Retained<T> vars and managed by Scope and ScopeRet instances. This is
inspired by V8's Local/Handle and EscapableHandleScope stuff.

Each function has at top-level scope a Scope object, which saves the stack
position. Then any non-captured locals are allocated through Local instances,
which advance the stack and hold a pointer to the cell. At the end of the func
the Scope goes out of scope and resets the stack pointer, invalidating all the
Local instances used in the meantime.

For return values, a ScopeRetVal or ScopeRet<T> is used instead, which allocates
a cell on the stack before its scope opens, which is still readable on the
calling function's scope. The function then returns a RetVal which points to
that value. Currently must call scope.escape(foo) but may be able to elide that.

Captured variables are individually allocated in GC'd Cells on the heap,
themselves kept alive on the locals stack until they're passed as a list of
Cell references into the Function object at creation time. This eats up a few
extra words per captured variable, but usually there aren't that many so
probably fine. And it means there's no worry about dependencies if multiple
closures capture overlapping subsets of variables -- each captured var has
its own GC-managed lifetime.


# NaN-boxing

NaN boxing makes use of the NaN space in double-precision floating point
numbers, so variable-type values can be stored in a single 64-bit word.
If the top 13 bits are all set, we have a "signalling NaN" that can't be
produced by natural operations, so we can use the remaining 51 bits for
a type tag and a payload.

For 32-bit targets like WebAssembly this leaves you plenty of space for
a 32-bit pointer! However on 64-bit native targets it's more worrisome.

SpiderMonkey uses NaN boxing with 4 bits of tag space and up to 47 bits of
pointer addressing, which is enough for current x86-64 code in user space, but
can fail on AArch64.

JSC uses a different format, with a bias added to doubles to shift NaNs
around so that pointers always have 0x0000 in the top 16 bits (giving you
48 bits of addressing, still potentially problematic), 0xffff for 32-bit
ints, and 0x0001..0xfffe for the shifted doubles.

The biggest differences between the two are that SpiderMonkey's system is
double-biased (you don't have to manipulate a double to get it in/out) and
JSC's is object-biased (you don't have to manipulate a pointer to get it
in/out). Since most JS code uses more objects than doubles, that may be a win.
On 32-bit it makes less difference, but is still maybe nicer for letting
you do a single 32-to-64 extend opcode instead of extending and then ORing
the tag. It does mean that the -Infinity value has to be stored as another
NaN value.

* [SpiderMonkey thinking of changing their boxing format](https://bugzilla.mozilla.org/show_bug.cgi?id=1401624)
* [JSC's format](https://github.com/adobe/webkit/blob/master/Source/JavaScriptCore/runtime/JSValue.h#L307)

Currently I'm using a format similar to SpiderMonkey's but using only 3 bits
for the tag (8 types), so top 16 bits is (signalling nan + tag) and bottom
48 bits is the pointer payload.

One downside is that the constants for the tag checks are large, and get
encoded at every check site. Alternately could right-shift by 48 and compare
against smaller constants, but that's probably an extra clock cycle vs extra
bytes in the binary.

# Naive implementation

Let's assume the runtime is implemented in C++. Naive implementation for totally
polymorphic code looks something like:
* [aotjs_runtime.h](aotjs_runtime.h)
* [aotjs_runtime.cpp](aotjs_runtime.cpp)

Example hand-compiled programs using it:
* [samples/gc.cpp](samples/gc.cpp)
* [samples/closure.js](samples/closure.js) -> [samples/closure.cpp](samples/closure.cpp)
* [samples/retval.js](samples/retval.js) -> [samples/retval.cpp](samples/retval.cpp)

Next steps (runtime):
* use PropIndex* instead of Val as property keys
* make sure hash map behaves right in properties

Next steps (features):
* provide string <-> number conversions
* exercise the Val <-> int and double conversions
* add operator overloads on Val for arithmetic
* add arrays and use operator[] for access of both arrays and obj props
* implement 'console' with at least basic 'log' and 'error' methods

Next steps (exceptions):
* signal exceptions with an explicit return value (or let emscripten do it?)
* provide a way to catch exceptions
* throw exceptions on various conditions

Next steps (compiler):
* pick a JavaScript-in-JavaScript parser
* write a JS/node app that takes a parsed AST and translates to C++
* start building tiny test programs and see what breaks or needs adding
