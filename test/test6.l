
fn spawn_units(wave) {
  print("spawn_units");
  spawn(ProtossZealot, Player7, wave, "TestLocation");
}

fn main() {
  print("1");
  spawn_units(5);
  print("2");
  
  while (true) {
    poll_events();
  }
}
