#include "../aotjs_runtime.h"

#include <iostream>

using namespace AotJS;

int main() {
  // Variables hoisted...
  Scope scope;

  {
    Scope scope2;

    Local work;
    Local play;
    Local retval;

    *work = new Function(
      "work",
      0, // argument count
      // no scope capture
      [] (Function& func, Frame& frame) -> RetVal {
        ScopeRetVal scope;
        return scope.escape(new String("work"));
      }
    );

    *play = new Function(
      "play",
      0, // argument count
      [] (Function& func, Frame& frame) -> RetVal {
        ScopeRetVal scope;
        return scope.escape(new String("play"));
      }
    );

    // todo: operator overloading on Val
    *retval = *(*work->call(Null(), {}) + *play->call(Null(), {}));

    // should say "workplay"
    std::cout << "should say 'workplay': " << retval->dump() << "\n";

    std::cout << "before gc\n";
    std::cout << engine().dump();
    std::cout << "\n\n";

  }

  engine().gc();

  std::cout << "after gc\n";
  std::cout << engine().dump();
  std::cout << "\n";

  return 0;
}
