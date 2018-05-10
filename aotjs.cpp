#include "aotjs_runtime.h"

int main() {
  auto root = new aotjs_object();
  aotjs_heap heap(root);

  heap.gc();
  return 0;
}
