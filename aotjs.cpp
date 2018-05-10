#include "aotjs_runtime.h"

int main() {
  auto root = new aotjs::aotjs_object();
  aotjs::aotjs_heap heap(root);

  heap.gc();
  return 0;
}
