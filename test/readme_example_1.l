#src test.scx

global foo = 10;

fn main() {
  var bar = 3;

  while(bar--) {
    add_resource(Player1, Minerals, foo + 2);
    foo = foo + 10;
  }
}
