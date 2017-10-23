#src test.scx

unit LurkerType1 {
  Health = 10,
  Burrowed = true
}

fn main() {
  spawn(ZergLurker, Player1, 1, "TestLocation", LurkerType1);
}