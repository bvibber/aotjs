#include "../aotjs_runtime.h"

#include <iostream>

using namespace AotJS;

int main() {
  // To protect them from being GC'd, local variables are kept on a
  // stack and we use Local smart pointers to manage the stack record's
  // lifetime.
  //
  // A 'Local' otherwise works like a 'Val*', and is bit-equivalent.
  Scope scope;
  Local func;

  // Register the function!
  func = new Function(
    "work",
    1, // argument count
    // no scope capture
    [] (Function& func, Local this_, ArgList args) -> Local {
      // Functions which can return a value must allocate stack space for the
      // retval, Or Else.
      ScopeRetVal scope;

      // Fetch the arguments into local Capture (Val*) refs, which can be
      // either read/modified or passed on to a
      // The function arity must be correct!
      // Attempting to read beyond the actual number will be invalid.
      //
      // If it were captured by a lamdba, we'd have to copy it into locals below.
      Local root(args[0]);

      // JavaScript local variable definitions are hoisted to the top.
      // They are all initialized to Undefined() when allocated on the stack.
      // These are variant wrappers (Val) which are also retained while on
      // the live-scope-call stack in the Local wrappers.
      Local obj;
      Local objname;
      Local propname;
      Local propval;
      Local unused;
      Local notpropname;

      // Now our function body actually starts!
      // Remember JS variable bindings are modeled as pointers to Val cells,
      // which we are filling with object pointers.
      obj = new Object();
      objname = new String("an_obj");
      propname = new String("propname");
      propval = new String("propval");
      unused = new String("unused");

      // This string is a different instance from the earlier "propname" one,
      // and should not survive GC.
      notpropname = new String(std::string("prop") + std::string("name"));

      // Retain a couple strings on an object
      // @todo this would be done with a wrapper interface probably
      obj->asObject().setProp(propname, propval);

      root->asObject().setProp(objname, obj);

      return scope.escape(Undefined());
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
