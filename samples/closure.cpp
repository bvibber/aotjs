#include "../aotjs_runtime.h"

#include <iostream>

using namespace AotJS;

Val work(Engine *engine, Function *func, Frame *frame);
Val work_func(Engine *engine, Function *func, Frame *frame);

Val work(Engine *engine, Function *func, Frame *frame) {
  auto captures = func->captures();

  // Get our arguments, if any.
  // Fill them out to the expected minimum number.
  auto args = frame->args();

  // Variable hoisting!
  // Conceptually we allocate all the locals at the start of the scope.
  // They'll all be filled with the JS `undefined` value initially.
  auto scope = engine->newScope(4);
  auto locals = scope->locals();

  // Pull the locals in as pointers into the scope's locals array.
  // They can be modified by later captures as long as they still
  // reference this scope.
  auto a = &locals[0];
  auto b = &locals[1];
  auto func = &locals[2];
  auto anon_retval = &locals[3];

  // function declarations happen at the top of the scope too.
  // This is where we capture the `b` variable's location, knowing
  // its actual value can change.
  *func = engine->newFunction(
    scope,     // parent scope
    work_func, // implementatoin
    "func",    // name
    0,         // arity
    {b}        // captures
  );

  // Now we get to the body of the function:
  *a = engine->newString("a");
  *b = engine->newString("b");

  std::cout << "should say 'b': " << b->dump() << "\n";

  // Make the call!
  auto retval = engine->call(*func, Null(), {});

  // should say "b plus one"
  std::cout << "should say 'b plus one': " << b->dump() << "\n";

  engine->popScope(scope);
  return Undefined();
}

// The closure function...
Val work_func(Engine *engine, Scope *scope) {
  auto args = scope->args();
  auto captures = scope->captures();

  // vars
  auto b = &captures[0];

  // replace the variable in the parent scope
  *b = engine->newString("b plus one");

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
