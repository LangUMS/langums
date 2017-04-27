fn main() {
  spawn(TerranMarine, Player1, 1, "TestLocation");
  modify(TerranMarine, Player1, 1, Health, 50, "TestLocation");
  order(TerranMarine, Player1, Move, "TestLocation", "TestLocation2");
 
  while (true) {
    poll_events();
  }
}
