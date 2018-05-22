#include "../aotjs_runtime.h"

#include <iostream>

using namespace AotJS;

int main() {
  Scope scope;

  // Hoist all variable declarations
  Local start;
  Local iterate_mandelbrot;
  Local x0, x1, y0, y1;
  Local cols, rows;
  Local maxIters;
  Local row, y, str, col, x, iters;
  Local end;

  start = engine().now();

  iterate_mandelbrot = new Function(
    "iterate_mandelbrot",
    3, // argument count
    // no scope capture
    [] (Function& func, Local this_, ArgList args) -> Local {
      ScopeRetVal scope;

      Local cx(args[0]);
      Local cy(args[1]);
      Local maxIters(args[2]);

      Local zx, zy, i, new_zx;

      zx = 0;
      zy = 0;

      for (i = 0; i < maxIters && (zx * zx + zy * zy) < 4.0; i++) {
        Scope subscope;
        new_zx = zx * zx - zy * zy + cx;
        zy = 2 * zx * zy + cy;
        zx = new_zx;
      }

      return scope.escape(i);
    }
  );

  x0 = -2.5; x1 = 1; y0 = -1; y1 = 1;
  cols = 72; rows = 24;
  maxIters = 1000;

  for (row = 0; row < rows; row++) {
    Scope scopeRow;

    y = (row / rows) * (y1 - y0) + y0;
    str = new String("");

    for (col = 0; col < cols; col++) {
      Scope scopeCol;

      x = (col / cols) * (x1 - x0) + x0;
      iters = iterate_mandelbrot->call(Null(), {x, y, maxIters});

      if (iters == 0) {
        str += new String(".");
      } else if (iters == 1) {
        str += new String("%");
      } else if (iters == 2) {
        str += new String("@");
      } else if (iters == maxIters) {
        str += new String(" ");
      } else {
        str += new String("#");
      }
    }
    std::cout << str->dump() << "\n";
  }

  end = engine().now() - start;
  std::cout << end->dump() << " milliseconds runtime\n";

  return 0;
}
