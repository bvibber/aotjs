#include "aotjs_runtime.h"

#include <iostream>

using namespace AotJS;

Val work(Engine *engine, Function *func, Frame *frame) {
  auto args = frame->args();
  auto locals = scope->locals();
  auto root = &args[0];
  auto obj = &args[1];
  auto objname = &args[2];
  auto propname = &args[3];
  auto propval = &args[4];
  auto unused = &args[5];

  *obj = engine->newObject(nullptr);
  *objname = engine->newString("an_obj");
  *propname = engine->newString("propname");
  *propval = engine->newString("propval");
  *unused = engine->newString("unused");

  // Retain a couple strings on an object
  // @todo this would be done with a wrapper interface probably
  obj->asObject()->setProp(propname, propval);

  root->asObject()->setProp(objname, obj);

  return Undefined();
}

int main() {
  Engine engine;

  // Register the function!
  auto func = engine.newFunction(
    engine.newScope(nullptr, 5),
    work,
    "work",
    1,
    {}
  );

  auto retval = engine.call(func, {engine.getRoot()});

  std::cout << "before gc\n";
  std::cout << engine.dump();
  std::cout << "\n\n";

  engine.gc();

  std::cout << "after gc\n";
  std::cout << engine.dump();
  std::cout << "\n";

  return 0;
}
