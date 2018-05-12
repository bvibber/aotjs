#include "../aotjs_runtime.h"

#include <iostream>

using namespace AotJS;

int main() {
  Engine engine;

  // todo make a default object on the engine?
  engine.setRoot(new Object(&engine, nullptr));

  // Register the function!
  auto func = new Function(
    &engine,
    [] (Engine *engine, Function *func, Frame *frame) -> Val {
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

      *obj = new Object(engine, nullptr);
      *objname = new String(engine, "an_obj");
      *propname = new String(engine, "propname");
      *propval = new String(engine, "propval");
      *unused = new String(engine, "unused");

      // This string is a different instance from the earlier "propname" one,
      // and should not survive GC.
      *notpropname = new String(engine, std::string("prop") + std::string("name"));

      // Retain a couple strings on an object
      // @todo this would be done with a wrapper interface probably
      obj->asObject()->setProp(*propname, *propval);

      root->asObject()->setProp(*objname, *obj);

      return Undefined();
    },
    "work",
    1, // argument count
    5, // number of locals
    nullptr, // no scope capture
    {} // no args
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
