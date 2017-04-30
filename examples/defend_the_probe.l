#define DEBUG true
#define TEN_SECONDS 8000

fn main() {
  print("Our heroes enter the arena.");
  spawn_heroes();
  print("The probe will spawn in 10 seconds, prepare...");
  
  if (!DEBUG) {
    sleep(TEN_SECONDS);
  }
  
  set_alliance(AllPlayers, Player7, Ally);
  set_alliance(Player7, AllPlayers, Ally);
  
  set_alliance(Player7, Player8, Enemy);
  set_alliance(Player8, Player7, Enemy);
 
  spawn(ProtossProbe, Player7, 1, HeroesSpawn);
 
  center_view(AllPlayers, HeroesSpawn);
  ping(AllPlayers, HeroesSpawn);
    
  print("The probe has spawned, protect it with your life!");
  sleep(2000);
  
  pick_random_target();
  
  while (true) {
    move_loc(ProtossProbe, Player7, AnyLocation, ProbeLocation); // center ProbeLocation on the probe
    
    if (probe_reached_target) {
      pick_random_target();
      probe_reached_target = false;
    }
    
    poll_events();
  }
}

// globals
global current_wave = 1;
global probe_reached_target = false;
//

// shop logic

global marine_shop_lock[8];
global zergling_shop_lock[8];

bring(Player1, AtLeast, 1, AllUnits, BuyMarines) => {
  print("hello world");
}

accumulate(Player1, AtLeast, 10, Minerals),
bring(Player1, AtLeast, 1, AllUnits, BuyMarines) => {
  if (marine_shop_lock[0]) {
    return;
  }

  take_resource(Player1, Minerals, 10);
  spawn(TerranMarine, Player1, 1, BuyMarines);
  print("You purchased a marine, take good care of it.");
  marine_shop_lock[0] = true;
}

bring(Player1, Exactly, 0, AllUnits, BuyMarines) => {
  marine_shop_lock[0] = false;
}

accumulate(Player1, AtLeast, 10, Minerals),
bring(Player1, AtLeast, 1, AllUnits, BuyZerglings) => {
  if (zergling_shop_lock[0]) {
    return;
  }

  take_resource(Player1, Minerals, 10);
  spawn(ZergZergling, Player1, 1, BuyZerglings);

  print("Your purchased a zergling, send that beast to the slaughter.");
  zergling_shop_lock[0] = true;
}

bring(Player1, Exactly, 0, AllUnits, BuyZerglings) => {
  zergling_shop_lock[0] = false;
}

// end shop logic

fn spawn_next_wave<Loc>(Loc) {
  current_wave++;
  print("They are coming for the probe, protect it!");
  
  spawn(ZergZergling, Player8, current_wave * 3, Loc);
  
  if (current_wave <= 3) {
    return;
  }

  spawn(ZergZergling, Player8, current_wave * 2, Loc);
  spawn(ZergHydralisk, Player8, current_wave, Loc);
  
  if (current_wave <= 5) {
    return;
  }
  
  spawn(ZergUltralisk, Player8, current_wave / 5, Loc);  
}

countdown(Exactly, 40) => {
  set_countdown(39);

  if (random() < 127) {
    spawn_next_wave(EnemySpawn1);
  } else {
    spawn_next_wave(EnemySpawn2);
  }
  
  attack_at(ProbeLocation);
}

countdown(Exactly, 35) => {
  set_countdown(34);
  attack_at(ProbeLocation);
}

countdown(Exactly, 30) => {
  set_countdown(29);
  attack_at(ProbeLocation);
}

countdown(Exactly, 25) => {
  set_countdown(24);
  attack_at(ProbeLocation);
}

countdown(Exactly, 20) => {
  set_countdown(19);
  attack_at(ProbeLocation);
}

countdown(Exactly, 15) => {
  set_countdown(14);
  attack_at(ProbeLocation);
}

countdown(Exactly, 10) => {
  set_countdown(9);
  attack_at(ProbeLocation);
}

countdown(Exactly, 5) => {
  set_countdown(4);
  attack_at(ProbeLocation);
}

fn spawn_if_present<Player, Unit, Loc>(Player, Unit, Loc) {
  if (is_present(Player)) {
    spawn(Unit, Player, 1, Loc);
    add_resource(Player, Minerals, 10);
    run_ai_script(Player, TurnOnSharedVisionOfPlayer7WithCurrentPlayer);
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

// probe random walk

countdown(Exactly, 0),
bring(Player7, Exactly, 1, ProtossProbe, ProbeTarget1) => {
  probe_reached_target = true;
}

countdown(Exactly, 0),
bring(Player7, Exactly, 1, ProtossProbe, ProbeTarget2) => {
  probe_reached_target = true;
}

countdown(Exactly, 0),
bring(Player7, Exactly, 1, ProtossProbe, ProbeTarget3) => {
  probe_reached_target = true;
}

countdown(Exactly, 0),
bring(Player7, Exactly, 1, ProtossProbe, ProbeTarget4) => {
  probe_reached_target = true;
}

countdown(Exactly, 0),
bring(Player7, Exactly, 1, ProtossProbe, ProbeTarget5) => {
  probe_reached_target = true;
}

fn pick_random_target() {
  if (probe_reached_target) {
    return;
  }

  print("Oh no, the probe is off to somewhere again...");
  set_countdown(45);
  
  var rnd = random();
  
  if (rnd <= 50) {
    order(ProtossProbe, Player7, Move, AnyLocation, ProbeTarget1);
    return;
  }
  
  if (rnd <= 100) {
    order(ProtossProbe, Player7, Move, AnyLocation, ProbeTarget2);
    return;
  }
  
  if (rnd <= 150) {
    order(ProtossProbe, Player7, Move, AnyLocation, ProbeTarget3);
    return;
  }
  
  if (rnd <= 200) {
    order(ProtossProbe, Player7, Move, AnyLocation, ProbeTarget4);
    return;
  }
  
  order(ProtossProbe, Player7, Move, AnyLocation, ProbeTarget5);
  attack_at(ProbeTarget5);
}

// end probe random walk