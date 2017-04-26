global timer = 5;

countdown(Exactly, 0) => {
  print("Countdown expired!");
  move(TerranMarine, Player1, 1, "TestLocation", "TestLocation2");
}

fn main() {
  set_countdown(timer);
  spawn(TerranMarine, Player1, 1, "TestLocation");
 
  while (true) {
    poll_events();
  }
}
