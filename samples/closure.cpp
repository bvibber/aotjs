#include "../aotjs_runtime.h"

#include <iostream>

using namespace AotJS;

int main() {
  AotJS::Engine engine;

  // Register the function!
  auto work = new Function(
    engine,
    // Use a lambda for source prettiness.
    // Must be no C++ captures so we can turn it into a raw function pointer!
    [] (Engine& aEngine, Function& aFunc, Frame& aFrame) -> Val {
      // Variable hoisting!
      // Conceptually we allocate all the locals at the start of the scope.
      // They'll all be filled with the JS `undefined` value initially.
      //
      // We allocate them on the stack frame if they're not captured, but
      // note we allocate a lexical scope first for those which are captured
      // by the closure function. That scope will live on separately after
      // the current function ends.
      Scope* const _scope1 = new Scope(aEngine, aFunc.scope(), 1);
      Val& _scopeval = aFrame.local(0);
      _scopeval = _scope1; // meh make this prettier
      Val& b = _scope1->local(0);

      Val& a = aFrame.local(1);
      Val& func = aFrame.local(2);
      Val& anon_retval = aFrame.local(3);

      // function declarations happen at the top of the scope too.
      // This is where we capture the `b` variable's location, knowing
      // its actual value can change.
      func = new Function(aEngine,
        // implementation
        [] (Engine& engine, Function& func, Frame& frame) -> Val {
          // Note we cannot use C++'s captures here -- they're not on GC heap and
          // would turn our call reference into a fat pointer, which we don't want.
          //
          // The capture gives you a reference into one of the linked Scopes,
          // which are retained via a chain referenced by the Function object.
          Val& b = func.capture(0);

          // replace the variable in the parent scope
          b = new String(engine, "b plus one");

          return Undefined();
        },
        "func",    // name
        0,         // arg arity
        0,         // locals
        *_scope1,  // lexical scope
        {&b}       // captures
      );

      // Now we get to the body of the function:
      a = new String(aEngine, "a");
      b = new String(aEngine, "b");

      std::cout << "should say 'b': " << b.dump() << "\n";

      // Make the call!
      anon_retval = aEngine.call(func, Null(), {});

      // should say "b plus one"
      std::cout << "should say 'b plus one': " << b.dump() << "\n";

      return Undefined();
    },
    "work",
    1,        // argument count
    5         // locals count
    // no closures
  );

  engine.call(work, Null(), {});

  std::cout << "before gc\n";
  std::cout << engine.dump();
  std::cout << "\n\n";

  engine.gc();

  std::cout << "after gc\n";
  std::cout << engine.dump();
  std::cout << "\n";

  return 0;
}
