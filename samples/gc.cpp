#include "../aotjs_runtime.h"

#include <iostream>

using namespace AotJS;

int main() {
  Engine engine;

  // Register the function!
  Local func = new Function(
    engine,
    [] (Engine& engine, Function& func, Frame& frame) -> Local {
      // Fetch the arguments. The function arity must be correct!
      // Attempting to read beyond the actual number will be invalid.
      // Can skip this call if no references to args.
      //
      // Note we use a Local here, not a Val&, because the binding is
      // local to the function body.
      Local root = frame.arg(0);

      // Local variable definitions are hoisted to the top.
      // They are all initialized to Undefined().
      // These are a
      Local obj;
      Local objname;
      Local propname;
      Local propval;
      Local unused;
      Local notpropname;

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
    1 // argument count
    // no scope capture
  );

  engine.call(func, Null(), {engine.root()});

  std::cout << "before gc\n";
  std::cout << engine.dump();
  std::cout << "\n\n";

  engine.gc();

  std::cout << "after gc\n";
  std::cout << engine.dump();
  std::cout << "\n";

  return 0;
}
