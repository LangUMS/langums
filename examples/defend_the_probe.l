global current_wave = 1;
global probe_spawned = false;

fn spawn_next_wave<Loc>(Loc) {
  current_wave++;
  print("They are coming for the probe, protect it!");
  
  spawn(ZergZergling, Player8, current_wave * 3, Loc);
  order(ZergZergling, Player8, Attack, Loc, ProbeLocation);
  
  if (current_wave >= 4) {
    spawn(ZergHydralisk, Player8, current_wave, Loc);
    order(ZergHydralisk, Player8, Attack, Loc, ProbeLocation);
  }
}

countdown(AtLeast, 20),
commands(Player8, Exactly, 0, ZergZergling) => {
  if (random() < 127) {
    spawn_next_wave(EnemySpawn1);
  } else {
    spawn_next_wave(EnemySpawn2);
  }
}

countdown(Exactly, 1) => {
  if (!probe_spawned) {
    spawn(ProtossProbe, Player7, 1, HeroesSpawn);
    move_loc(ProtossProbe, Player7, HeroesSpawn, ProbeLocation);
    pick_random_target(HeroesSpawn);
    probe_spawned = true;
  }
}

fn spawn_if_present<Player, Unit, Loc>(Player, Unit, Loc) {
  if (is_present(Player)) {
    spawn(Unit, Player, 1, Loc);
  }
}

fn spawn_heroes() {
  spawn_if_present(Player1, HeroJimRaynorMarine, HeroesSpawn);
  spawn_if_present(Player2, HeroJimRaynorMarine, HeroesSpawn);
  spawn_if_present(Player3, HeroJimRaynorMarine, HeroesSpawn);
  spawn_if_present(Player4, HeroJimRaynorMarine, HeroesSpawn);
  spawn_if_present(Player5, HeroJimRaynorMarine, HeroesSpawn);
  spawn_if_present(Player6, HeroJimRaynorMarine, HeroesSpawn);
}

fn main() {
  set_alliance(Player1, Player7, Ally);
  set_alliance(Player2, Player7, Ally);
  set_alliance(Player3, Player7, Ally);
  set_alliance(Player4, Player7, Ally);
  set_alliance(Player5, Player7, Ally);
  set_alliance(Player6, Player7, Ally);
  
  spawn_heroes();
  print("The probe will spawn in 10 seconds, prepare...");
  set_countdown(10);

  while (true) {
    poll_events();
  }
}

// probe random walk

bring(Player7, Exactly, 1, ProtossProbe, ProbeTarget1),
countdown(Exactly, 0) => {
  pick_random_target(ProbeTarget1);
}

bring(Player7, Exactly, 1, ProtossProbe, ProbeTarget2),
countdown(Exactly, 0) => {
  pick_random_target(ProbeTarget2);
}

bring(Player7, Exactly, 1, ProtossProbe, ProbeTarget3),
countdown(Exactly, 0) => {
  pick_random_target(ProbeTarget3);
}

bring(Player7, Exactly, 1, ProtossProbe, ProbeTarget4),
countdown(Exactly, 0) => {
  pick_random_target(ProbeTarget4);
}

bring(Player7, Exactly, 1, ProtossProbe, ProbeTarget5),
countdown(Exactly, 0) => {
  pick_random_target(ProbeTarget5);
}

fn pick_random_target<SrcLoc>(SrcLoc) {
  print("Oh no, the probe is off to somewhere again...");

  var rnd = random();
  
  if (rnd <= 50) {
    order(ProtossProbe, Player7, Move, SrcLoc, ProbeTarget1);
    return;
  }
  
  if (rnd <= 100) {
    order(ProtossProbe, Player7, Move, SrcLoc, ProbeTarget2);
    return;
  }
  
  if (rnd <= 150) {
    order(ProtossProbe, Player7, Move, SrcLoc, ProbeTarget3);
    return;
  }
  
  if (rnd <= 200) {
    order(ProtossProbe, Player7, Move, SrcLoc, ProbeTarget4);
    return;
  }
  
  order(ProtossProbe, Player7, Move, SrcLoc, ProbeTarget5);
  set_countdown(60);
}

// end probe random walk