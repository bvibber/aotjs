#include "aotjs_runtime.h"

#include <iostream>

int main() {
  AotJS::Engine engine;

  auto root = engine.getRoot();
  auto obj = engine.newObject(nullptr);
  auto name = engine.newString("propname");
  auto val = engine.newString("propval");
  auto unused = engine.newString("unused");

  root->setProp(name, val);

  std::cout << "before gc\n";
  std::cout << engine.dump();
  std::cout << "\n\n";

  engine.gc();

  std::cout << "after gc\n";
  std::cout << engine.dump();
  std::cout << "\n";

  return 0;
}
