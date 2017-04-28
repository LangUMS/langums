unit LurkerType1 {
  Health = 10,
  Burrowed = 1
}

fn main() {
  spawn(ZergLurker, Player1, 1, "TestLocation", LurkerType1);
  
  while (true) {
    poll_events();
  }
}
