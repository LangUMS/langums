fn bar(arg) {
  print("hello world");
}

fn main() {
  var foo = rnd256();
  
  if (foo > 128) {
    print("lucky you");
    spawn(TerranMarine, Player1, 1, "TestLocation");
  } else {
    print("try again next time");
    spawn(TerranMarine, Player2, 1, "TestLocation");
  }
  
  bar(1);
}
