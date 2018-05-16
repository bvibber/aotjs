#include "../aotjs_runtime.h"

#include <iostream>

using namespace AotJS;

int main() {
  // All non-captured local variables will live in this scope,
  // to protect them from being GC'd. We must pop it at the end
  // of the function.
  //
  // Beware that temporary variables in function call parameters and such
  // may be GC'd in some cases, so always stick things immediately in here
  // on allocation!
  Local func;

  // Register the function!
  *func = new Function(
    "work",
    1, // argument count
    // no scope capture
    [] (Function& func, Frame& frame) -> Val {
      // Fetch the arguments. The function arity must be correct!
      // Attempting to read beyond the actual number will be invalid.
      // Can skip this call if no references to args.
      //
      // If it were captured, though, we'd have to copy it into locals below.
      Val* root = frame.arg(0);

      // Local variable definitions are hoisted to the top.
      // They are all initialized to Undefined() at Scope creation.
      // These are variant wrappers which are also retained while it's on
      // the live-scope-call stack.
      //
      // This could be made prettier in C++17 with destructuring and
      // templates for the item count, but let's be explicit for what the
      // low-level code does.
      Local obj;
      Local objname;
      Local propname;
      Local propval;
      Local unused;
      Local notpropname;

      // Now our function body actually starts!
      *obj = new Object();
      *objname = new String("an_obj");
      *propname = new String("propname");
      *propval = new String("propval");
      *unused = new String("unused");

      // This string is a different instance from the earlier "propname" one,
      // and should not survive GC.
      *notpropname = new String(std::string("prop") + std::string("name"));

      // Retain a couple strings on an object
      // @todo this would be done with a wrapper interface probably
      obj->asObject().setProp(*propname, *propval);

      root->asObject().setProp(*objname, *obj);

      return Undefined();
    }
  );

  func->call(Null(), {engine().root()});

  std::cout << "before gc\n";
  std::cout << engine().dump();
  std::cout << "\n\n";

  engine().gc();

  std::cout << "after gc\n";
  std::cout << engine().dump();
  std::cout << "\n";

  return 0;
}
