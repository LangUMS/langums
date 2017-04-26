global timer = 20;

fn main() {
  set_countdown(timer);
  spawn(TerranMarine, Player1, 1, "TestLocation");
  move(TerranMarine, Player1, 1, "TestLocation", "TestLocation2");
  
  while (true) {
    var x = 5;
  }
}
