#include "../aotjs_runtime.h"

#include <iostream>

using namespace AotJS;

Val work_body(Engine *engine, Function *func, Frame *frame) {
  // Fetch the argument list. The function arity must be correct!
  // Attempting to read beyond the actual number will be invalid.
  // Can skip this call if no references to args.
  auto root = frame->arg(0);

  auto obj = frame->local(0);
  auto objname = frame->local(1);
  auto propname = frame->local(2);
  auto propval = frame->local(3);
  auto unused = frame->local(4);
  auto notpropname = frame->local(5);

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

  return Undefined();
}

int main() {
  Engine engine;

  // Register the function!
  auto func = engine.newFunction(
    work_body,
    "work",
    1, // argument count
    5 // number of locals
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
