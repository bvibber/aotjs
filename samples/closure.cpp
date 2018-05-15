#include "../aotjs_runtime.h"

#include <iostream>

using namespace AotJS;

int main() {
  AotJS::Engine engine;

  auto work = engine.local();

  // Register the function!
  *work = new Function(
    engine,
    // Use a lambda for source prettiness.
    // Must be no C++ captures so we can turn it into a raw function pointer!
    [] (Engine& engine, Function& func_, Frame& frame) -> Val {
      // Variable hoisting!
      // Conceptually we allocate all the locals at the start of the scope.
      // They'll all be filled with the JS `undefined` value initially.
      //
      // We allocate them in Scopes, which are kept on a stack while we
      // run and, in the case of closure captures, can live on beyond
      // the function end.
      //
      // Todo make this prettier?
      // Todo have a better facility for immutable bindings.
      auto closure1 = engine.scope(1);

      auto a = engine.local();
      auto b = closure1->local(0);
      auto func = engine.local();

      // function declarations/definitions happen at the top of the scope too.
      // This is where we capture the `b` variable's location, knowing
      // its actual value can change.
      *func = new Function(engine,
        // implementation
        [] (Engine& engine, Function& func_, Frame& frame) -> Val {
          // Note we cannot use C++'s captures here -- they're not on GC heap and
          // would turn our call reference into a fat pointer, which we don't want.
          //
          // The capture gives you a reference into one of the linked Scopes,
          // which are retained via a chain referenced by the Function object.
          auto b = func_.capture(0);

          // replace the variable in the parent scope
          *b = new String(engine, "b plus one");

          return Undefined();
        },
        "func",    // name
        0,         // arg arity
        *closure1, // lexical scope
        {b}        // captures
      );

      // Now we get to the body of the function:
      *a = new String(engine, "a");
      *b = new String(engine, "b");

      std::cout << "should say 'b': " << b->dump() << "\n";

      // Make the call!
      engine.call(*func, Null(), {});

      // should say "b plus one"
      std::cout << "should say 'b plus one': " << b->dump() << "\n";

      return Undefined();
    },
    "work",
    1        // argument count
    // no closures
  );

  engine.call(*work, Null(), {});

  std::cout << "before gc\n";
  std::cout << engine.dump();
  std::cout << "\n\n";

  engine.gc();

  std::cout << "after gc\n";
  std::cout << engine.dump();
  std::cout << "\n";

  return 0;
}
