#include "../aotjs_runtime.h"

#include <iostream>

using namespace AotJS;

int main() {
  Local work;

  // Register the function!
  *work = new Function(
    "work",
    1, // argument count
    // no closures
    // Use a lambda for source prettiness.
    // Must be no C++ captures so we can turn it into a raw function pointer!
    [] (Function& func_, Frame& frame) -> Val {
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
      auto closure1 = retain<Scope>(1);

      Local a;
      Capture b = closure1->local(0);
      Local func;

      // function declarations/definitions happen at the top of the scope too.
      // This is where we capture the `b` variable's location, knowing
      // its actual value can change.
      *func = new Function(
        "func",    // name
        0,         // arg arity
        *closure1, // lexical scope
        {b},       // captures
        // implementation
        [] (Function& func, Frame& frame) -> Val {
          // Note we cannot use C++'s captures here -- they're not on GC heap and
          // would turn our call reference into a fat pointer, which we don't want.
          //
          // The capture gives you a reference into one of the linked Scopes,
          // which are retained via a chain referenced by the Function object.
          Capture b = func.capture(0);

          // replace the variable in the parent scope
          *b = new String("b plus one");

          return Undefined();
        }
      );

      // Now we get to the body of the function:
      *a = new String("a");
      *b = new String("b");

      std::cout << "should say 'b': " << b->dump() << "\n";

      // Make the call!
      func->call(Null(), {});

      // should say "b plus one"
      std::cout << "should say 'b plus one': " << b->dump() << "\n";

      return Undefined();
    }
  );

  work->call(Null(), {});

  std::cout << "before gc\n";
  std::cout << engine().dump();
  std::cout << "\n\n";

  engine().gc();

  std::cout << "after gc\n";
  std::cout << engine().dump();
  std::cout << "\n";

  return 0;
}
