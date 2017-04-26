accumulate(Player1, AtLeast, 25, Minerals) => {
  print("We have the minerals!");
}

elapsed_time(AtLeast, 5) => {
  print("5 seconds have elapsed!");
}

fn main() {  
  add_resource(Player1, Minerals, 25);
  
  while (true) {
    poll_events();
  }
}
