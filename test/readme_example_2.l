#src test.scx

fn get_unit_count(wave) {
  return 10 + wave * 5;
}

fn spawn_units(count, wave) {
  if (wave == 1) {
    spawn(TerranMarine, Player1, count, "TestLocation");
    return;
  }
  
  if (wave == 2) {
    spawn(ProtossZealot, Player1, count, "TestLocation");
    return;
  }
  
  if (wave == 3) {
    spawn(ZergZergling, Player1, count, "TestLocation");
    return;
  }
}

fn main() {
  var wave = 2;
  var count = get_unit_count(wave);
  spawn_units(count, wave);
}