#include "../aotjs_runtime.h"

#include <iostream>

using namespace AotJS;

int main() {
  Scope scope;
  Local work;

  // Register the function!
  work = new Function(
    "work",
    1, // argument count
    // no closures
    // Use a lambda for source prettiness.
    // Must be no C++ captures so we can turn it into a raw function pointer!
    [] (Function& func_, Local this_, ArgList args) -> Local {
      ScopeRetVal scope;

      // Variable hoisting!
      // Conceptually we allocate all the locals at the start of the scope.
      // They'll all be filled with the JS `undefined` value initially.
      //
      // We allocate most vars in Locals, which are wrapped pointers into a
      // stack of Vals that are kept alive while. In the case of closure
      // captures, they're allocated in a GC'd HeapCell object on the heap,
      // which itself is kept on the same stack.
      //
      // JS variable bindings are all pointers, either wrapped with a Local
      // or Retained<T> smart pointer to the stack or straight as a Binding
      // pointer into one of those.
      Retained<Cell> _b;

      Local a;
      Local& b =_b->binding();
      Local func;

      // function declarations/definitions happen at the top of the scope too.
      // This is where we capture the `b` variable's location, knowing
      // its actual value can change.
      func = new Function(
        "func",    // name
        0,         // arg arity
        {_b},      // captures
        // implementation
        [] (Function& func, Local this_, ArgList args) -> Local {
          // Allocate local scope for the return value.
          ScopeRetVal scope;

          // Note we cannot use C++'s captures here -- they're not on GC heap and
          // would turn our call reference into a fat pointer, which we don't want.
          //
          // The capture binding gives you a reference into one of the linked
          // heap Cells, which are retained via a list in the Function object.
          Local& b = func.capture(0).binding();

          // replace the variable in the parent scope
          b = new String("b plus one");

          return scope.escape(Undefined());
        }
      );

      // Now we get to the body of the function:
      a = new String("a");
      b = new String("b");

      std::cout << "should say 'b': " << b->dump() << "\n";

      // Make the call!
      func->call(Null(), {});

      // should say "b plus one"
      std::cout << "should say 'b plus one': " << b->dump() << "\n";

      return scope.escape(Undefined());
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
