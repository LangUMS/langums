accumulate(Player1, AtLeast, 25, Minerals) => {
  print("We have the minerals!");
}

elapsed_time(AtLeast, 5) => {
  print("5 seconds have elapsed!");
}

fn give_minerals<T>(T, qty) {
  add_resource(T, Minerals, qty);
}

fn main() {  
  give_minerals(Player1, 50);
  
  while (true) {
    poll_events();
  }
}
