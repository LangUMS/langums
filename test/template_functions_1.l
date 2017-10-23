#src test.scx

commands_least(Player3, TerranMarine, "TestLocation") => {
  print("you have the most marines", Player3);
}

killed_most(Player4, ZergQueen) => {
  print("Player 4 has killed the most queens");
}

most_resources(Player5, Minerals) => {
  print("Player5 has the most minerals.");
}

opponents(Player1, AtLeast, 1) => {
  print("i have at least one opponent");
}

fn spawn_units<T>(T, qty) {
  talking_portrait(Player1, T, 5);
  spawn(T, Player1, qty, "TestLocation");
  order(T, Player1, Move, "TestLocation", "TestLocation2");
}

fn main() {
  spawn_units(TerranMarine, 5);
  
  while(true) {
    poll_events();
  }
}
