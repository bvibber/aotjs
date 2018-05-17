#include "../aotjs_runtime.h"

#include <iostream>

using namespace AotJS;

int main() {
  Scope scope;
  Local report;

  // Register the function!
  *report = new Function(
    "report",
    3, // argument count
    [] (Function& func, Frame& frame) -> RetVal {
      ScopeRetVal scope;

      Binding a = frame.arg(0);
      Binding b = frame.arg(1);
      Binding c = frame.arg(2);

      std::cout << a->dump() << ", " << b->dump() << ", " << c->dump() << "\n";

      return scope.escape(Undefined());
    }
  );

  report->call(Null(), {new String("a"), new String("b"), new String("c")});
  report->call(Null(), {new String("a"), new String("b")});
  report->call(Null(), {1, 2, 3});
  report->call(Null(), {new Object(), 3.5, Null()});

  std::cout << "before gc\n";
  std::cout << engine().dump();
  std::cout << "\n\n";

  engine().gc();

  std::cout << "after gc\n";
  std::cout << engine().dump();
  std::cout << "\n";

  return 0;
}
