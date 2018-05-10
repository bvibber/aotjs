aotjs : aotjs_runtime.h aotjs_runtime.cpp aotjs.cpp
	clang++ -g -std=c++17 -o aotjs aotjs_runtime.cpp aotjs.cpp
