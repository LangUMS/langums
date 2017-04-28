props Marine1 {
  Health = 50
}

fn main() {
  spawn(TerranMarine, Player1, 1, "TestLocation", Marine1);
  
  while (true) {
    poll_events();
  }
}
