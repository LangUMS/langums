fn main() {
  spawn(TerranMarine, Player1, 1, "TestLocation");
  give(TerranMarine, Player1, Player2, 1, "TestLocation");

  var foo = rnd256();
  
  if (foo > 128) {
    print("lucky you");
  }
  
  if (foo <= 128) {
    print("try again next time");
  }
}
