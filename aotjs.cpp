#include "aotjs_runtime.h"

int main() {
  auto root = new AotJS::Object();
  AotJS::Heap heap(root);

  heap.gc();
  return 0;
}
