#include "../aotjs_runtime.h"

#include <iostream>

using namespace AotJS;

int main() {
  AotJS::Engine engine;

  auto& locals = engine.pushScope(1);
  Val& work = locals[0];

  // Register the function!
  work = new Function(
    engine,
    // Use a lambda for source prettiness.
    // Must be no C++ captures so we can turn it into a raw function pointer!
    [] (Engine& aEngine, Function& aFunc, Frame& aFrame) -> Local {
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
      auto _locals = engine.pushScope(2);
      auto _closure1 = engine.pushScope(1);

      Val& a = _locals[0];
      Val& b = _closure1[0];
      Val& func = _locals[1];

      // function declarations/definitions happen at the top of the scope too.
      // This is where we capture the `b` variable's location, knowing
      // its actual value can change.
      func = new Function(aEngine,
        // implementation
        [] (Engine& engine, Function& func, Frame& frame) -> Local {
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
        _closure1, // lexical scope
        {&b}       // captures
      );

      // Now we get to the body of the function:
      a = new String(aEngine, "a");
      b = new String(aEngine, "b");

      std::cout << "should say 'b': " << b.dump() << "\n";

      // Make the call!
      aEngine.call(func, Null(), {});

      // should say "b plus one"
      std::cout << "should say 'b plus one': " << b.dump() << "\n";

      // We could do this via a destructor on a wrapper reference class.
      // But the generated code, if low level, needs to be explicit so
      // let's not go too crazy.
      aEngine.popScope();
      aEngine.popScope();

      return Undefined();
    },
    "work",
    1        // argument count
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

  engine.popScope();
  return 0;
}
