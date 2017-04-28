fn main() {
  spawn(TerranMarine, Player2, 1, "TestLocation");
  spawn(TerranDropship, Player2, 1, "TestLocation");
  run_ai_script(Player2, EnterTransport, "TestLocation");
  
  while (true) {
    poll_events();
  }
}
