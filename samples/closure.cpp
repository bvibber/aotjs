#include "../aotjs_runtime.h"

#include <iostream>

using namespace AotJS;

Val work_body(Engine *engine, Function *func, Frame *frame);
Val func_body(Engine *engine, Function *func, Frame *frame);

Val work_body(Engine *aEngine, Function *aFunc, Frame *aFrame) {
  // Variable hoisting!
  // Conceptually we allocate all the locals at the start of the scope.
  // They'll all be filled with the JS `undefined` value initially.
  //
  // We allocate them on the stack frame if they're not captured, but
  // note we allocate a lexical scope first for those which are captured
  // by the closure function. That scope will live on separately after
  // the current function ends.
  Scope* const _scope1 = new Scope(aEngine, aFunc->scope(), 1);
  *(aFrame->local(0)) = _scope1;
  Val* const b = _scope1->local(0);

  Val* const a = aFrame->local(1);
  Val* const func = aFrame->local(2);
  Val* const anon_retval = aFrame->local(3);

  // function declarations happen at the top of the scope too.
  // This is where we capture the `b` variable's location, knowing
  // its actual value can change.
  *func = new Function(aEngine,
    func_body, // implementation
    "func",    // name
    0,         // arg arity
    0,         // locals
    _scope1,   // lexical scope
    {b}        // captures
  );

  // Now we get to the body of the function:
  *a = new String(aEngine, "a");
  *b = new String(aEngine, "b");

  std::cout << "should say 'b': " << b->dump() << "\n";

  // Make the call!
  *anon_retval = aEngine->call(*func, Null(), {});

  // should say "b plus one"
  std::cout << "should say 'b plus one': " << b->dump() << "\n";

  return Undefined();
}

// The closure function...
Val func_body(Engine *engine, Function *func, Frame *frame) {
  // Unlike the locals and args arrays, captures gives you a ptr to a ptr
  Val* const b = func->capture(0);

  // replace the variable in the parent scope
  *b = new String(engine, "b plus one");

  return Undefined();
}

int main() {
  AotJS::Engine engine;

  // todo make a default object on the engine?
  engine.setRoot(new Object(&engine, nullptr));

  // Register the function!
  auto work = new Function(
    &engine,
    work_body,
    "work",
    1,        // argument count
    5,        // locals count
    nullptr,  // closure scope
    {}        // captures
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
