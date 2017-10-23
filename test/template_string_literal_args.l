#src test.scx

fn spawn_units<T, L>(T, L, qty) {
  talking_portrait(Player1, T, 5);
  spawn(T, Player1, qty, L);
  order(T, Player1, Move, L, "TestLocation2");
}

fn main() {
  spawn_units(TerranMarine, "TestLocation", 5);
}