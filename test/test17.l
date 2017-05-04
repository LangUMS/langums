for <PlayerId> in (Player1, Player2, Player3, Player4, Player5, Player6) {
  bring(PlayerId, AtLeast, 1, AllUnits, TestLocation) => {
    print("The SCV is here!");
  }
}

fn main() {
  for <PlayerId> in (Player1, Player2, Player3, Player4, Player5, Player6) {
    print("hello", PlayerId);
  }
}
