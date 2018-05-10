#include "../aotjs_runtime.h"

#include <iostream>

using namespace AotJS;

Val work_func(Scope *scope) {
  auto b = scope->findCapture(0);

  // replace the variable in the parent scope
  *b = scope->newString("b plus one");

  return Undefined();
}

Val work(Scope *scope) {
  // Concepturally we allocate all the locals at the start of the scope.
  scope->allocLocals(2);

  // Pull the locals in as pointers into the scope's locals array.
  // They can be modified by later captures as long as they still
  // reference this scope.
  auto a = scope->findLocal(0);
  auto b = scope->findLocal(1);

  *a = scope->newString("a");
  *b = scope->newString("b");

  std::cout << "should say 'b': " << b->dump() << "\n";

  // Capture the b variable's location into the closure scope...
  auto retval = scope->call(work_func, {}, {b});

  // should say "b plus one"
  std::cout << "should say 'b plus one': " << b->dump() << "\n";

  return Undefined();
}

int main() {
  AotJS::Engine engine;

  engine.call(work, {}, {});

  std::cout << "before gc\n";
  std::cout << engine.dump();
  std::cout << "\n\n";

  engine.gc();

  std::cout << "after gc\n";
  std::cout << engine.dump();
  std::cout << "\n";

  return 0;
}
