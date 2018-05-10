#include "aotjs_runtime.h"

int main() {
  AotJS::Heap heap;

  auto obj = heap.newObject(nullptr);
  auto str = heap.newString(L"this is a nice string");

  heap.gc();
  return 0;
}
