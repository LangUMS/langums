accumulate(Player1, AtLeast, 25, Minerals) => {
  print("We have the minerals!");
}

fn main() {  
  add_resource(Player1, Minerals, 25);
  
  while (true) {
    poll_events();
  }
}
