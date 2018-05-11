#include "../aotjs_runtime.h"

#include <iostream>

using namespace AotJS;

Val work_body(Engine *engine, Function *func, Frame *frame);
Val func_body(Engine *engine, Function *func, Frame *frame);

Val work_body(Engine *aEngine, Function *aFunc, Frame *aFrame) {
  // Get our captures, if any.
  auto captures = aFunc->captures();

  // Get our arguments, if any.
  auto args = aFrame->args();

  // Variable hoisting!
  // Conceptually we allocate all the locals at the start of the scope.
  // They'll all be filled with the JS `undefined` value initially.
  auto scope = aEngine->pushScope(4);
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
  *func = aEngine->newFunction(
    func_body, // implementation
    "func",    // name
    0,         // arity
    {b}        // captures
  );

  // Now we get to the body of the function:
  *a = aEngine->newString("a");
  *b = aEngine->newString("b");

  std::cout << "should say 'b': " << b->dump() << "\n";

  // Make the call!
  auto retval = aEngine->call(*func, Null(), {});

  // should say "b plus one"
  std::cout << "should say 'b plus one': " << b->dump() << "\n";

  aEngine->popScope();
  return Undefined();
}

// The closure function...
Val func_body(Engine *engine, Function *func, Frame *frame) {
  // Unlike the locals and args arrays, captures gives you a ptr to a ptr
  auto captures = func->captures();
  auto b = captures[0];

  // replace the variable in the parent scope
  *b = engine->newString("b plus one");

  return Undefined();
}

int main() {
  AotJS::Engine engine;

  // Register the function!
  auto work = engine.newFunction(
    work_body,
    "work",
    1,       // argument count
    {}       // no captures
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
