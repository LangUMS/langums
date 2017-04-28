fn main() {
  set_mission_objectives("""Hello world!
This is a multi-line string""");
 
  spawn(TerranMarine, Player1, 1, "TestLocation");
  spawn(TerranMarine, Player2, 1, "TestLocation");
  set_alliance(Player1, Player2, Ally);
  set_alliance(Player2, Player1, Ally);
  
  while (true) {
    poll_events();
  }
}
