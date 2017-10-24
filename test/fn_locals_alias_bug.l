#src test.scx

global y = 13;

fn foo(x, y) {
  x = 42;
  y++;
}

fn bar(x, y) {
  x = 4;
  y = 5;
}

fn main() {
  var x = 8;
  foo(x, y);
  bar(y, x);
}
