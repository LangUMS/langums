#src test.scx

bring(Player1, Exactly, 5, TerranMarine, "TestLocation"),
accumulate(Player1, AtLeast, 25, Gas) => {
  print("The marines have arrived!");
}

fn main() {
  while(true) {
    poll_events();
  }
}
