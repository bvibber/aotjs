// z(n+1) = z(n)^2 + c
function iterate_mandelbrot(cx, cy, maxIters) {
  var zx = 0, zy = 0;
  for (var i = 0; i < maxIters && (zx * zx + zy * zy) < 4.0; i++) {
    var new_zx = zx * zx - zy * zy + cx;
    zy = 2 * zx * zy + cy;
    zx = new_zx;
  }
  return i;
}

var x0 = -2.5, x1 = 1, y0 = -1, y1 = 1;
var cols = 72, rows = 24;
var maxIters = 1000;

for (var row = 0; row < rows; row++) {
  var y = (row / rows) * (y1 - y0) + y0;
  var str = '';
  for (var col = 0; col < cols; col++) {
    var x = (col / cols) * (x1 - x0) + x0;
    var iters = iterate_mandelbrot(x, y, maxIters);
    if (iters == 0) {
      str += '.';
    } else if (iters == 1) {
      str += '%';
    } else if (iters == 2) {
      str += '@';
    } else if (iters == maxIters) {
      str += ' ';
    } else {
      str += '#';
    }
  }
  console.log(str);
}