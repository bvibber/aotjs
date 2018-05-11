#include "../aotjs_runtime.h"

#include <iostream>

using namespace AotJS;

Val work_body(Engine *engine, Function *func, Frame *frame) {
  // Fetch the argument list. The function arity must be correct!
  // Attempting to read beyond the actual number will be invalid.
  // Can skip this call if no references to args.
  auto args = frame->args();
  auto root = &args[0];

  // Open a scope with local variables.
  // Can skip this call if no locals are used?
  auto scope = engine->pushScope(6);

  auto locals = scope->locals();
  auto obj = &locals[0];
  auto objname = &locals[1];
  auto propname = &locals[2];
  auto propval = &locals[3];
  auto unused = &locals[4];
  auto notpropname = &locals[5];

  *obj = engine->newObject(nullptr);
  *objname = engine->newString("an_obj");
  *propname = engine->newString("propname");
  *propval = engine->newString("propval");
  *unused = engine->newString("unused");

  // This string is a different instance from the earlier "propname" one,
  // and should not survive GC.
  *notpropname = engine->newString(std::string("prop") + std::string("name"));

  // Retain a couple strings on an object
  // @todo this would be done with a wrapper interface probably
  obj->asObject()->setProp(*propname, *propval);

  root->asObject()->setProp(*objname, *obj);

  // We could do this with a destructor too, but if we generate LLVM
  // bitcode directly we'll have to do that manually anyway.
  engine->popScope();

  return Undefined();
}

int main() {
  Engine engine;

  // Register the function!
  auto func = engine.newFunction(
    work_body,
    "work",
    1,       // argument count
    {}       // no captures
  );

  auto retval = engine.call(func, Null(), {engine.root()});

  std::cout << "before gc\n";
  std::cout << engine.dump();
  std::cout << "\n\n";

  engine.gc();

  std::cout << "after gc\n";
  std::cout << engine.dump();
  std::cout << "\n";

  return 0;
}
