fn get_unit_count(wave) {
  return 10 + wave * 5;
}

fn spawn_units(count, wave) {
  if (wave == 1) {
    spawn(TerranMarine, Player7, count, "MobsSpawn");
  }
  
  if (wave == 2) {
    spawn(ProtossZealot, Player7, count, "MobsSpawn");
  }
  
  if (wave == 3) {
    spawn(ZergZergling, Player7, count, "MobsSpawn");
  }
}

fn main() {
  var wave = 4;
  var count = get_unit_count(wave);
  spawn_units(count, wave);
}
