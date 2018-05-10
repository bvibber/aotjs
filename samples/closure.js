function work() {
  var a = "a";
  var b = "b";

  function func() {
    b = "b plus one";
  }

  // prints "b"
  console.log(b);

  func();

  // prints "b plus one"
  console.log(b);
}

work();
