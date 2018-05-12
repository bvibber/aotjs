#include "../aotjs_runtime.h"

#include <iostream>

using namespace AotJS;

int main() {
  Engine engine;

  // Register the function!
  Val func = new Function(
    engine,
    [] (Engine& engine, Function& func, Frame& frame) -> Val {
      // Fetch the argument list. The function arity must be correct!
      // Attempting to read beyond the actual number will be invalid.
      // Can skip this call if no references to args.
      Val& root = frame.arg(0);

      Val& obj = frame.local(0);
      Val& objname = frame.local(1);
      Val& propname = frame.local(2);
      Val& propval = frame.local(3);
      Val& unused = frame.local(4);
      Val& notpropname = frame.local(5);

      // Now our function body actually starts!
      obj = new Object(engine);
      objname = new String(engine, "an_obj");
      propname = new String(engine, "propname");
      propval = new String(engine, "propval");
      unused = new String(engine, "unused");

      // This string is a different instance from the earlier "propname" one,
      // and should not survive GC.
      notpropname = new String(engine, std::string("prop") + std::string("name"));

      // Retain a couple strings on an object
      // @todo this would be done with a wrapper interface probably
      obj.asObject().setProp(propname, propval);

      root.asObject().setProp(objname, obj);

      return Undefined();
    },
    "work",
    1, // argument count
    5  // number of locals
    // no scope capture
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
