#src test.scx

fn main() {  
  var foo = 100;
  add_resource(Player1, Minerals, foo / 30);
  ping(Player1, "TestLocation");
  
  while (true) {
    poll_events();
  }
}
