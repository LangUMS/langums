#define DEBUG true

global current_wave = 1;
global probe_spawned = false;

fn calculate_wave_size() {
  var activePlayers = is_present();
  return current_wave * activePlayers * 2;
}

fn spawn_next_wave<Loc>(Loc) {
  current_wave++;
  print("They are coming for the probe, protect it!");
  
  var waveSize = calculate_wave_size();
  if (DEBUG && waveSize > 0) {
    print("debug: spawning at least one ling");
  }
  
  spawn(ZergZergling, Player8, waveSize, Loc);
  
  if (current_wave <= 3) {
    return;
  }

  spawn(ZergZergling, Player8, waveSize, Loc);
  spawn(ZergHydralisk, Player8, waveSize, Loc);
  
  if (current_wave <= 5) {
    return;
  }
  
  spawn(ZergUltralisk, Player8, waveSize / 5, Loc);  
}

countdown(Exactly, 25) => {
  set_countdown(24);

  if (random() < 127) {
    spawn_next_wave(EnemySpawn1);
  } else {
    spawn_next_wave(EnemySpawn2);
  }
}

countdown(Exactly, 1) => {
  if (!probe_spawned) {
    spawn(ProtossProbe, Player7, 1, HeroesSpawn);
    center_view(AllPlayers, HeroesSpawn);
    ping(AllPlayers, HeroesSpawn);
    
    print("The probe has spawned, protect it with your life!");
    sleep(2000);
    
    probe_spawned = true;
    pick_random_target(HeroesSpawn);
  }
}

fn spawn_if_present<Player, Unit, Loc>(Player, Unit, Loc) {
  if (is_present(Player)) {
    spawn(Unit, Player, 1, Loc);
  }
}

fn attack_at<Loc>(Loc) { // order all enemies to attack at a location
  order(AllUnits, Player8, Attack, AnyLocation, Loc);
}

fn spawn_heroes() {
  spawn_if_present(Player1, HeroJimRaynorMarine, HeroesSpawn);
  spawn_if_present(Player2, HeroJimRaynorMarine, HeroesSpawn);
  spawn_if_present(Player3, HeroJimRaynorMarine, HeroesSpawn);
  spawn_if_present(Player4, HeroJimRaynorMarine, HeroesSpawn);
  spawn_if_present(Player5, HeroJimRaynorMarine, HeroesSpawn);
  spawn_if_present(Player6, HeroJimRaynorMarine, HeroesSpawn);
  center_view(AllPlayers, HeroesSpawn);
}

fn main() {
  print("The probe will spawn in 10 seconds, prepare...");
  set_countdown(10);
  
  spawn_heroes();

  set_alliance(Player1, Player7, Ally);
  set_alliance(Player2, Player7, Ally);
  set_alliance(Player3, Player7, Ally);
  set_alliance(Player4, Player7, Ally);
  set_alliance(Player5, Player7, Ally);
  set_alliance(Player6, Player7, Ally);
  
  while (true) {
    move_loc(ProtossProbe, Player7, AnyLocation, ProbeLocation); // center ProbeLocation on the probe
    poll_events();
  }
}

// probe random walk

global probe_has_target = false;

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
  set_countdown(30);
  
  var rnd = random();
  
  if (rnd <= 50) {
    order(ProtossProbe, Player7, Move, SrcLoc, ProbeTarget1);
    attack_at(ProbeTarget1);
    return;
  }
  
  if (rnd <= 100) {
    order(ProtossProbe, Player7, Move, SrcLoc, ProbeTarget2);
    attack_at(ProbeTarget2);
    return;
  }
  
  if (rnd <= 150) {
    order(ProtossProbe, Player7, Move, SrcLoc, ProbeTarget3);
    attack_at(ProbeTarget3);
    return;
  }
  
  if (rnd <= 200) {
    order(ProtossProbe, Player7, Move, SrcLoc, ProbeTarget4);
    attack_at(ProbeTarget4);
    return;
  }
  
  order(ProtossProbe, Player7, Move, SrcLoc, ProbeTarget5);
  attack_at(ProbeTarget5);
}

// end probe random walk