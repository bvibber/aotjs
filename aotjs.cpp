#include "aotjs_runtime.h"

#include <iostream>

int main() {
  AotJS::Heap heap;

  auto root = heap.getRoot();
  auto obj = heap.newObject(nullptr);
  auto name = heap.newString("propname");
  auto val = heap.newString("propval");
  auto unused = heap.newString("unused");

  root->setProp(name, val);

  std::cout << "before gc\n";
  std::cout << heap.dump();
  std::cout << "\n\n";

  heap.gc();

  std::cout << "after gc\n";
  std::cout << heap.dump();
  std::cout << "\n";

  return 0;
}
