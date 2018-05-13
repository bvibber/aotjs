#include "../aotjs_runtime.h"

#include <iostream>

using namespace AotJS;

int main() {
  Engine engine;

  // All non-captured local variables will live in this scope,
  // to protect them from being GC'd. We must pop it at the end
  // of the function.
  //
  // Beware that temporary variables in function call parameters and such
  // may be GC'd in some cases, so always stick things immediately in here
  // on allocation!
  auto& scope = engine.pushScope(1);
  Val& func = scope[0];

  // Register the function!
  func = new Function(
    engine,
    [] (Engine& engine, Function& func, Frame& frame) -> Val {
      auto scope = engine.pushScope(6);

      // Fetch the arguments. The function arity must be correct!
      // Attempting to read beyond the actual number will be invalid.
      // Can skip this call if no references to args.
      //
      // If it were captured, though, we'd have to copy it into locals below.
      Val& root = frame.arg(0);

      // Local variable definitions are hoisted to the top.
      // They are all initialized to Undefined().
      // These are variant wrappers which are also retained while in scope.
      Val& obj = scope[0];
      Val& objname = scope[1];
      Val& propname = scope[2];
      Val& propval = scope[3];
      Val& unused = scope[4];
      Val& notpropname = scope[5];

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

      engine.popScope();

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
