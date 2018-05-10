#include "aotjs_runtime.h"

#include <iostream>

AotJS::Val work(AotJS::Scope *scope) {
  auto root = scope->findArg(0);

  auto obj = scope->newObject(nullptr);
  auto objname = scope->newString("an_obj");
  auto propname = scope->newString("propname");
  auto propval = scope->newString("propval");
  auto unused = scope->newString("unused");

  // Retain a couple strings on an object
  obj->setProp(propname, propval);

  root->asObject()->setProp(objname, obj);

  return AotJS::Undefined();
}

int main() {
  AotJS::Engine engine;

  engine.call(work, {engine.getRoot()}, {});

  std::cout << "before gc\n";
  std::cout << engine.dump();
  std::cout << "\n\n";

  engine.gc();

  std::cout << "after gc\n";
  std::cout << engine.dump();
  std::cout << "\n";

  return 0;
}
