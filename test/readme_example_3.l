#src test.scx

global my_array[8];

fn main() {
  my_array[Player1] = 42;
  my_array[Player2] = 0x07;
  
  if (my_array[Player2] == 15) {
    print("foo");
  }
  
  var foo[4];
  foo[0] = 10;
  foo[1] = 13 * 2;
}