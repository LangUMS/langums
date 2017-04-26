fn main() {
  spawn(TerranMarine, Player1, 1, "TestLocation");
  move(TerranMarine, Player1, 1, "TestLocation", "TestLocation2");
  
  while (true) {
    poll_events();
  }
}
