// Defend the Probe
// map file: defend_the_probe.scx

#define DEBUG false
#define FOUR_SECONDS 4000

// prices
#define MARINE_COST 50
#define MEDIC_COST 75
#define VULTURE_COST 100
#define TANK_COST 250
#define SCV_COST 30
//

// globals
global current_wave = 0;
global probe_spawned = false;
//

fn main() {
  setup_alliances();

  center_view(AllPlayers, MapCenter);
  spawn_heroes();

  set_mission_objectives("""- Don't let the probes die.'
- You can buy units at the beacons.
- Upgrade your units with gas.
  """);

  show_leaderboard_goal("Probes left to save", Points, 100, Custom);  

  if (!DEBUG)
  {
    sleep(FOUR_SECONDS);
    print("Buy units from the beacons.");
    ping(AllPlayers, BuyVultures);
    sleep(FOUR_SECONDS);
    ping(AllPlayers, Upgrades);
    center_view(AllPlayers, Upgrades);
    print("Buy upgrades from here.");
    sleep(FOUR_SECONDS);
  }

  add_resource(AllPlayers, Minerals, 100);
  add_resource(AllPlayers, Gas, 50);

  center_view(AllPlayers, MapCenter);
  ping(AllPlayers, MapCenter);
  print("Stop the probes from getting killed before reaching the beacons to earn gas.");
  
  set_countdown(10);
  clear_buffered_events();
  probe_spawned = true;

  while (true) {
    modify(AllUnits, Force1, 10, Health, 100, HealStation);
    modify(AllUnits, Force1, 10, Energy, 100, HealStation);
    move(TerranEngineeringBay, AllPlayers, 1, AnyLocation, Upgrades);
    poll_events();
  }
}

// win/ lose conditions

score(Player7, Custom, AtLeast, 100) => {
  end(AllPlayers, Victory);
}

for <PlayerId> in (Player1, Player2, Player3, Player4, Player5, Player6) {
  commands(PlayerId, Exactly, 0, HeroJimRaynorMarine) => {
    if (!probe_spawned) {
      return;
    }

    print("You have died a glorious death. o7", PlayerId);
    end(PlayerId, Defeat);
  }
}

// end win/ lose conditions

// shop logic

global shop_locks[6];

for <PlayerId> in (Player1, Player2, Player3, Player4, Player5, Player6) {

  bring(PlayerId, Exactly, 0, AllUnits, BuyMarines),
  bring(PlayerId, Exactly, 0, AllUnits, BuyVultures),  
  bring(PlayerId, Exactly, 0, AllUnits, BuySiegeTanks),
  bring(PlayerId, Exactly, 0, AllUnits, BuySCV),    
  bring(PlayerId, Exactly, 0, AllUnits, BuyMedics) => {
    shop_locks[PlayerId] = false;
  }

  bring(PlayerId, AtLeast, 1, AllUnits, BuyMarines),
  accumulate(PlayerId, AtLeast, MARINE_COST, Minerals) => {
    if (shop_locks[PlayerId]) {
      return;
    }

    take_resource(PlayerId, Minerals, MARINE_COST);
    spawn(TerranMarine, PlayerId, 1, MapCenter);
    shop_locks[PlayerId] = true;
  }

  bring(PlayerId, AtLeast, 1, AllUnits, BuyMedics),
  accumulate(PlayerId, AtLeast, MEDIC_COST, Minerals) => {
    if (shop_locks[PlayerId]) {
      return;
    }

    take_resource(PlayerId, Minerals, MEDIC_COST);
    spawn(TerranMedic, PlayerId, 1, MapCenter);
    shop_locks[PlayerId] = true;
  }

  bring(PlayerId, AtLeast, 1, AllUnits, BuyVultures),
  accumulate(PlayerId, AtLeast, VULTURE_COST, Minerals) => {
    if (shop_locks[PlayerId]) {
      return;
    }

    take_resource(PlayerId, Minerals, VULTURE_COST);
    spawn(TerranVulture, PlayerId, 1, MapCenter);
    shop_locks[PlayerId] = true;
  }

  bring(PlayerId, AtLeast, 1, AllUnits, BuySiegeTanks),
  accumulate(PlayerId, AtLeast, TANK_COST, Minerals) => {
    if (shop_locks[PlayerId]) {
      return;
    }

    take_resource(PlayerId, Minerals, TANK_COST);
    spawn(TerranSiegeTankTankMode, PlayerId, 1, MapCenter);
    shop_locks[PlayerId] = true;
  }


  bring(PlayerId, AtLeast, 1, AllUnits, BuySCV),
  accumulate(PlayerId, AtLeast, SCV_COST, Minerals) => {
    if (shop_locks[PlayerId]) {
      return;
    }

    take_resource(PlayerId, Minerals, SCV_COST);
    spawn(TerranSCV, PlayerId, 1, MapCenter);
    shop_locks[PlayerId] = true;
  }

}

// end shop logic

// reward logic

deaths(Player8, AtLeast, 4, ZergZergling) => {
  remove_deaths(Player8, ZergZergling, 1);
  add_resource(AllPlayers, Minerals, 1);
}

deaths(Player8, AtLeast, 3, ZergHydralisk) => {
  remove_deaths(Player8, ZergHydralisk, 1);
  add_resource(AllPlayers, Minerals, 1);
}

deaths(Player8, AtLeast, 1, ZergUltralisk) => {
  remove_deaths(Player8, ZergUltralisk, 1);
  add_resource(AllPlayers, Minerals, 2);
}

deaths(Player8, AtLeast, 3, ZergInfestedTerran) => {
  remove_deaths(Player8, ZergInfestedTerran, 2);
  add_resource(AllPlayers, Minerals, 1);
}

// end reward logic

fn spawn_next_wave() {
  spawn(ZergZergling, Player8, 12, EnemySpawn1);
  spawn(ZergZergling, Player8, 12, EnemySpawn2);

  if (current_wave <= 8) {
    return;
  }

  spawn(ZergZergling, Player8, 8, EnemySpawn2);
  spawn(ZergHydralisk, Player8, 6, EnemySpawn1);

  if (current_wave <= 15) {
    return;
  }

  spawn(ZergZergling, Player8, 6, EnemySpawn2);
  spawn(ZergHydralisk, Player8, 3, EnemySpawn2);
  spawn(ZergUltralisk, Player8, 3, EnemySpawn2);

  if (current_wave <= 25) {
    return;
  }
  
  spawn(ZergHydralisk, Player8, 3, EnemySpawn1);
  spawn(ZergInfestedTerran, Player8, 8, EnemySpawn2);

  if (current_wave <= 30) {
    return;
  }

  spawn(ZergDefiler, Player8, 2, EnemySpawn1);
  spawn(ZergDefiler, Player8, 2, EnemySpawn2);

  if (current_wave >= 35) {
    current_wave = 34;
  }
}

countdown(Exactly, 1) => {
  set_countdown(27);

  current_wave++;
  spawn_next_wave();

  spawn_probes();

  var rnd = random();

  if (rnd < 100) {
    order(AllUnits, Player8, Patrol, EnemySpawn1, ProbeTarget3);
    order(AllUnits, Player8, Patrol, EnemySpawn2, ProbeTarget4);
    return;
  }

  if (rnd < 200) {
    order(AllUnits, Player8, Patrol, EnemySpawn1, ProbeTarget1);
    order(AllUnits, Player8, Patrol, EnemySpawn2, ProbeTarget2);
    return;
  }

  order(AllUnits, Player8, Patrol, AnyLocation, MapCenter);
}

fn spawn_if_present<PlayerId, Unit, Loc>(PlayerId, Unit, Loc) {
  if (is_present(PlayerId)) {
    spawn(Unit, PlayerId, 1, Loc);
    set_doodad(PlayerId, AllUnits, Disable, DisableClick);
  }
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
  spawn_if_present(Player1, HeroJimRaynorMarine, MapCenter);
  spawn_if_present(Player2, HeroJimRaynorMarine, MapCenter);
  spawn_if_present(Player3, HeroJimRaynorMarine, MapCenter);
  spawn_if_present(Player4, HeroJimRaynorMarine, MapCenter);
  spawn_if_present(Player5, HeroJimRaynorMarine, MapCenter);
  spawn_if_present(Player6, HeroJimRaynorMarine, MapCenter);
  
  center_view(AllPlayers, MapCenter);
}

// end initialization stuff

bring(Player8, AtLeast, 1, AllUnits, ProbeTarget1) => {
  order(AllUnits, Player8, Patrol, ProbeTarget1, MapCenter);
}

bring(Player8, AtLeast, 1, AllUnits, ProbeTarget2) => {
  order(AllUnits, Player8, Patrol, ProbeTarget2, MapCenter);
}

bring(Player8, AtLeast, 1, AllUnits, ProbeTarget3) => {
  order(AllUnits, Player8, Patrol, ProbeTarget3, MapCenter);
}

bring(Player8, AtLeast, 1, AllUnits, ProbeTarget4) => {
  order(AllUnits, Player8, Patrol, ProbeTarget4, MapCenter);
}

bring(Player7, AtLeast, 1, ProtossProbe, ProbeTarget1) => {
  add_resource(AllPlayers, Gas, 2);
  remove(ProtossProbe, Player7, 1, ProbeTarget1);
  add_score(AllPlayers, Custom, 1);
}

bring(Player7, AtLeast, 1, ProtossProbe, ProbeTarget2) => {
  add_resource(AllPlayers, Gas, 2);
  remove(ProtossProbe, Player7, 1, ProbeTarget2);
  add_score(AllPlayers, Custom, 1);
}

bring(Player7, AtLeast, 1, ProtossProbe, ProbeTarget3) => {
  add_resource(AllPlayers, Gas, 2);
  remove(ProtossProbe, Player7, 1, ProbeTarget3);
  add_score(AllPlayers, Custom, 1);
}

bring(Player7, AtLeast, 1, ProtossProbe, ProbeTarget4) => {
  add_resource(AllPlayers, Gas, 2);
  remove(ProtossProbe, Player7, 1, ProbeTarget4);
  add_score(AllPlayers, Custom, 1);
}

fn spawn_probes() {
  spawn(ProtossProbe, Player7, 6, MapCenter);

  var rnd = random();
  
  if (rnd <= 65) {
    ping(AllPlayers, ProbeTarget1);
    order(ProtossProbe, Player7, Move, MapCenter, ProbeTarget1);
    return;
  }
  
  if (rnd <= 130) {
    ping(AllPlayers, ProbeTarget2);
    order(ProtossProbe, Player7, Move, MapCenter, ProbeTarget2);
    return;
  }
  
  if (rnd <= 195) {
    ping(AllPlayers, ProbeTarget3);
    order(ProtossProbe, Player7, Move, MapCenter, ProbeTarget3);
    return;
  }

  ping(AllPlayers, ProbeTarget4);
  order(ProtossProbe, Player7, Move, MapCenter, ProbeTarget4);
}

