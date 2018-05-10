#include "aotjs_runtime.h"

#include <iostream>

int main() {
  AotJS::Engine engine;

  auto root = engine.getRoot();
  auto obj = engine.newObject(nullptr);
  auto objname = engine.newString("an_obj");
  auto propname = engine.newString("propname");
  auto propval = engine.newString("propval");
  auto unused = engine.newString("unused");

  obj->setProp(propname, propval);
  root->setProp(objname, obj);

  std::cout << "before gc\n";
  std::cout << engine.dump();
  std::cout << "\n\n";

  engine.gc();

  std::cout << "after gc\n";
  std::cout << engine.dump();
  std::cout << "\n";

  return 0;
}
