// Defend the Probe
// map file: defend_the_probe.scx

#define DEBUG false
#define FOUR_SECONDS 4000

// globals
global current_wave = 1;
global probe_target = 0;
//

fn main() {
  setup_alliances();

  center_view(AllPlayers, HeroesSpawn);
  spawn_heroes();

  set_mission_objectives("""- Don't let the probe die.'
- You can buy marines and zerglings from the beacons
  """);

  if (!DEBUG)
  {
    print("The probe will spawn in 15 seconds, prepare...");
    sleep(FOUR_SECONDS);
    print("Buy units from the beacons.");
    sleep(FOUR_SECONDS);
    ping(AllPlayers, Upgrades);
    center_view(AllPlayers, Upgrades);
    print("Buy upgrades from here.");
    sleep(FOUR_SECONDS);
  }

  center_view(AllPlayers, HeroesSpawn);
  ping(AllPlayers, HeroesSpawn);
  spawn(ProtossProbe, Player7, 1, HeroesSpawn);
  print("The probe has spawned, protect it with your life!");
  sleep(2000);
  
  add_resource(AllPlayers, Minerals, 10);
  
  set_countdown(35);
  clear_buffered_events();

  while (true) {
    move_loc(ProtossProbe, Player7, AnyLocation, ProbeLocation); // center ProbeLocation on the probe
    run_heal_stations();

    if (!probe_target) {
      pick_random_target();
    }
    poll_events();
  }
}

// heal stations

fn run_heal_stations() {
  modify(AllUnits, AllPlayers, 10, Health, 100, "HealStation");
}

// end heal stations

// lose conditions

commands(Player7, Exactly, 0, ProtossProbe) => {
  if (probe_target == 0) {
    return;
  }

  print("The probe is dead. You are losers.");
  sleep(2000);
  end(Player1, Defeat);
  end(Player2, Defeat);
  end(Player3, Defeat);
  end(Player4, Defeat);
  end(Player5, Defeat);
  end(Player6, Defeat);
}

for <PlayerId> in (Player1, Player2, Player3, Player4, Player5, Player6) {
  commands(Player7, Exactly, 0, ProtossProbe) => {
    if (probe_target == 0) {
      return;
    }

    print("You have died a glorious death. o7");
    end(PlayerId, Defeat);
  }
}

// end lose conditions

// shop logic

for <PlayerId> in (Player1, Player2, Player3, Player4, Player5, Player6) {
  accumulate(PlayerId, AtLeast, 10, Minerals),
  bring(PlayerId, AtLeast, 1, HeroJimRaynorMarine, BuyMarines) => {
    take_resource(PlayerId, Minerals, 10);
    spawn(TerranMarine, PlayerId, 1, ShopSpawn);
  }

  accumulate(PlayerId, AtLeast, 5, Minerals),
  bring(PlayerId, AtLeast, 1, HeroJimRaynorMarine, BuyZerglings) => {
    take_resource(PlayerId, Minerals, 5);
    spawn(ZergZergling, PlayerId, 1, ShopSpawn);
  }

  accumulate(PlayerId, AtLeast, 25, Minerals),
  bring(PlayerId, AtLeast, 1, HeroJimRaynorMarine, BuyZealots) => {
    take_resource(PlayerId, Minerals, 25);
    spawn(ProtossZealot, PlayerId, 1, ShopSpawn);
  }
}

// end shop logic

// reward logic

deaths(Player8, AtLeast, 1, ZergZergling) => {
  remove_deaths(Player8, ZergZergling, 1);
  add_resource(AllPlayers, Minerals, 1);
}

deaths(Player8, AtLeast, 1, ZergHydralisk) => {
  remove_deaths(Player8, ZergZergling, 1);
  add_resource(AllPlayers, Minerals, 1);
}

deaths(Player8, AtLeast, 1, ZergUltralisk) => {
  remove_deaths(Player8, ZergZergling, 1);
  add_resource(AllPlayers, Minerals, 3);
}

// end reward logic

fn spawn_next_wave() {
  var x = current_wave * 2;

  spawn(ZergZergling, Player8, x, EnemySpawn1);
  spawn(ZergZergling, Player8, x, EnemySpawn2);
  
  if (current_wave <= 3) {
    return;
  }

  spawn(ZergZergling, Player8, x, EnemySpawn2);
  spawn(ZergHydralisk, Player8, x, EnemySpawn1);
  
  if (current_wave <= 6) {
    return;
  }
  
  spawn(ZergZergling, Player8, x, EnemySpawn2);
  spawn(ZergUltralisk, Player8, x, EnemySpawn2);
}

countdown(Exactly, 34) => {
  set_countdown(33);

  current_wave++;
  print("They are coming for the probe, protect it!");
  spawn_next_wave(EnemySpawn1);
  
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

countdown(Exactly, 1) => {
  set_countdown(35);
}

fn spawn_if_present<Player, Unit, Loc>(Player, Unit, Loc) {
  if (is_present(Player)) {
    spawn(Unit, Player, 1, Loc);
  }
}

fn attack_at<Loc>(Loc) { // order all enemies to attack at a location
  order(AllUnits, Player8, Attack, AnyLocation, Loc);
}

// initialization stuff

fn setup_alliances() {
  set_alliance(AllPlayers, Player7, Ally);
  set_alliance(Player7, AllPlayers, Ally);
  
  set_alliance(Player7, Player8, Enemy);
  set_alliance(Player8, Player7, Enemy);

  set_vision(Player1, Player7, true);
  set_vision(Player2, Player7, true);
  set_vision(Player3, Player7, true);
  set_vision(Player4, Player7, true);
  set_vision(Player5, Player7, true);
  set_vision(Player6, Player7, true);
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

// end initialization stuff

// probe random walk

bring(Player7, Exactly, 1, ProtossProbe, ProbeTarget1) => {
  if (probe_target != 1) {
    return;
  }

  probe_target = 0;
  if (DEBUG) {
    print("debug: probe reached target 1");
  }
}

bring(Player7, Exactly, 1, ProtossProbe, ProbeTarget2) => {
  if (probe_target != 2) {
    return;
  }

  probe_target = 0;
  if (DEBUG) {
    print("debug: probe reached target 2");
  }
}

bring(Player7, Exactly, 1, ProtossProbe, ProbeTarget3) => {
  if (probe_target != 3) {
    return;
  }
  
  probe_target = 0;
  if (DEBUG) {
    print("debug: probe reached target 3");
  }
}

bring(Player7, Exactly, 1, ProtossProbe, ProbeTarget4) => {
  if (probe_target != 4) {
    return;
  }

  probe_target = 0;
  if (DEBUG) {
    print("debug: probe reached target 4");
  }
}

bring(Player7, Exactly, 1, ProtossProbe, ProbeTarget5) => {
  if (probe_target != 5) {
    return;
  }

  probe_target = 0;
  if (DEBUG) {
    print("debug: probe reached target 5");
  }
}

fn pick_random_target() {
  var rnd = random();
  
  if (rnd <= 50) {
    order(ProtossProbe, Player7, Move, AnyLocation, ProbeTarget1);
    probe_target = 1;
    return;
  }
  
  if (rnd <= 100) {
    order(ProtossProbe, Player7, Move, AnyLocation, ProbeTarget2);
    probe_target = 2;
    return;
  }
  
  if (rnd <= 150) {
    order(ProtossProbe, Player7, Move, AnyLocation, ProbeTarget3);
    probe_target = 3;
    return;
  }
  
  if (rnd <= 200) {
    order(ProtossProbe, Player7, Move, AnyLocation, ProbeTarget4);
    probe_target = 4;
    return;
  }
  
  order(ProtossProbe, Player7, Move, AnyLocation, ProbeTarget5);
  probe_target = 5;
}

// end probe random walk
