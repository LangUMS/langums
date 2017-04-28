fn spawn_units<T>(T, qty) {
  talking_portrait(T, 5);
  spawn(T, Player1, qty, "TestLocation");
  order(T, Player1, Move, "TestLocation", "TestLocation2");
}

fn main() {
  spawn_units(TerranMarine, 5);
}
