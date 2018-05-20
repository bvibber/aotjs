#include "../aotjs_runtime.h"

#include <iostream>

using namespace AotJS;

int main() {
  {
    Scope scope;
    Local report;

    // Register the function!
    report = new Function(
      "report",
      3, // argument count
      [] (Function& func, Local this_, ArgList args) -> Local {
        ScopeRetVal scope;

        // this might work in c++17 mode with suitable iteators added
        //Local& [a, b, c] = args;

        Local& a = args[0];
        Local& b = args[1];
        Local& c = args[2];

        std::cout << a->dump() << ", " << b->dump() << ", " << c->dump() << "\n";

        return scope.escape(Undefined());
      }
    );

    report->call(Null(), {new String("a"), new String("b"), new String("c")});
    report->call(Null(), {new String("a"), new String("b")});
    report->call(Null(), {1, 2, 3});
    report->call(Null(), {new Object(), 3.5, Null()});
  }

  std::cout << "before gc\n";
  std::cout << engine().dump();
  std::cout << "\n\n";

  engine().gc();

  std::cout << "after gc\n";
  std::cout << engine().dump();
  std::cout << "\n";

  return 0;
}
