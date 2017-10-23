#src defend_the_probe.scx

// Defend the Probe

#define DEBUG false
#define THREE_SECONDS 4000

// prices
#define MARINE_COST 50
#define MEDIC_COST 75
#define VULTURE_COST 100
#define FIREBAT_COST 130
#define TANK_COST 250
#define SCV_COST 35
//

// globals
global current_wave = 0;
global probe_spawned = false;
//

fn main() {
  setup_alliances();

  center_view(AllPlayers, MapCenter);
  spawn_heroes();

  set_mission_objectives("""- Don't let the probes die until they deliver the cargo.'
- You can buy units at the beacons.
- Upgrade your units with gas you get from deliveries.
  """);

  show_leaderboard_goal("deliveries left to make", Points, 32, Custom);  
  leaderboard_show_cpu(Disable);

  if (!DEBUG)
  {
    sleep(THREE_SECONDS);
    print("Buy units from the beacons.");
    ping(AllPlayers, BuyMarines);
    ping(AllPlayers, BuySCV);
    ping(AllPlayers, BuyMedics);
    ping(AllPlayers, BuyFirebats);
    ping(AllPlayers, BuySiegeTanks);
    ping(AllPlayers, BuyVultures);
    sleep(THREE_SECONDS);
    ping(AllPlayers, Upgrades);
    center_view(AllPlayers, Upgrades);
    print("Buy upgrades from here.");
    sleep(THREE_SECONDS);
  }

  center_view(AllPlayers, MapCenter);
  ping(AllPlayers, MapCenter);
  print("Stop the probes from getting killed before making a delivery to earn gas.");
  sleep(3000);

  print("Probes take cargo from these four beacons...");
  ping(AllPlayers, ProbeTarget1);
  ping(AllPlayers, ProbeTarget2);
  ping(AllPlayers, ProbeTarget3);
  ping(AllPlayers, ProbeTarget4);
  sleep(2000);

  ping(AllPlayers, Delivery);
  print("...and deliver it here.");
  sleep(1000);

  add_resource(AllPlayers, Minerals, 100);
  add_resource(AllPlayers, Gas, 50);

  set_countdown(20);
  clear_buffered_events();
  probe_spawned = true;

  while (true) {
    move(TerranEngineeringBay, AllPlayers, 1, AnyLocation, Upgrades);
    poll_events();
  }
}

// heal station

bring(Player8, Exactly, 0, AllUnits, HealStation) => {
  modify(AllUnits, Force1, 10, Health, 100, HealStation);
  modify(AllUnits, Force1, 10, Energy, 100, HealStation);
}

//

// spawn objective pickups

for <TargetLoc> in (ProbeTarget1, ProbeTarget2, ProbeTarget3, ProbeTarget4) {
  bring(Player7, Exactly, 0, PowerupDataDisk, TargetLoc) => {
    spawn(PowerupDataDisk, Player7, 1, TargetLoc);
  }
}

// win/ lose conditions

for <PlayerId> in (Player1, Player2, Player3, Player4, Player5, Player6) {
  commands(PlayerId, Exactly, 0, HeroJimRaynorMarine) => {
    if (!probe_spawned) {
      return;
    }

    print("You have died a glorious death. o7", PlayerId);
    end(PlayerId, Defeat);
  }

  score(Player7, Custom, AtLeast, 32) => {
    end(PlayerId, Victory);
  }
}

// end win/ lose conditions

// shop logic

for <PlayerId> in (Player1, Player2, Player3, Player4, Player5, Player6) {

  bring(PlayerId, AtLeast, 1, AllUnits, BuyMarines),
  accumulate(PlayerId, AtLeast, MARINE_COST, Minerals) => {
    take_resource(PlayerId, Minerals, MARINE_COST);
    spawn(TerranMarine, PlayerId, 1, MapCenter);
    move(AllUnits, PlayerId, 100, BuyMarines, BuyMarinesReturn);
  }

  bring(PlayerId, AtLeast, 1, AllUnits, BuyMedics),
  accumulate(PlayerId, AtLeast, MEDIC_COST, Minerals) => {
    take_resource(PlayerId, Minerals, MEDIC_COST);
    spawn(TerranMedic, PlayerId, 1, MapCenter);
    move(AllUnits, PlayerId, 100, BuyMedics, BuyMedicsReturn);
  }

  bring(PlayerId, AtLeast, 1, AllUnits, BuyVultures),
  accumulate(PlayerId, AtLeast, VULTURE_COST, Minerals) => {
    take_resource(PlayerId, Minerals, VULTURE_COST);
    spawn(TerranVulture, PlayerId, 1, MapCenter);
    move(AllUnits, PlayerId, 100, BuyVultures, BuyVulturesReturn);
  }

  bring(PlayerId, AtLeast, 1, AllUnits, BuyFirebats),
  accumulate(PlayerId, AtLeast, FIREBAT_COST, Minerals) => {
    take_resource(PlayerId, Minerals, FIREBAT_COST);
    spawn(TerranFirebat, PlayerId, 1, MapCenter);
    move(AllUnits, PlayerId, 100, BuyFirebats, BuyFirebatsReturn);
  }

  bring(PlayerId, AtLeast, 1, AllUnits, BuySiegeTanks),
  accumulate(PlayerId, AtLeast, TANK_COST, Minerals) => {
    take_resource(PlayerId, Minerals, TANK_COST);
    spawn(TerranSiegeTankTankMode, PlayerId, 1, MapCenter);
    move(AllUnits, PlayerId, 100, BuySiegeTanks, BuySiegeTanksReturn);
  }

  bring(PlayerId, AtLeast, 1, AllUnits, BuySCV),
  accumulate(PlayerId, AtLeast, SCV_COST, Minerals) => {
    take_resource(PlayerId, Minerals, SCV_COST);
    spawn(TerranSCV, PlayerId, 1, MapCenter);
    move(AllUnits, PlayerId, 100, BuySCV, BuySCVReturn);
  }

}

// end shop logic

// reward logic

deaths(Player8, AtLeast, 2, ZergZergling) => {
  remove_deaths(Player8, ZergZergling, 2);
  add_resource(AllPlayers, Minerals, 1);
}

deaths(Player8, AtLeast, 1, ZergHydralisk) => {
  remove_deaths(Player8, ZergHydralisk, 1);
  add_resource(AllPlayers, Minerals, 3);
}

deaths(Player8, AtLeast, 1, ZergUltralisk) => {
  remove_deaths(Player8, ZergUltralisk, 1);
  add_resource(AllPlayers, Minerals, 4);
}

deaths(Player8, AtLeast, 3, ZergInfestedTerran) => {
  remove_deaths(Player8, ZergInfestedTerran, 3);
  add_resource(AllPlayers, Minerals, 2);
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
  set_countdown(26);

  current_wave++;
  spawn_next_wave();

  spawn_probe();

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

// initialization stuff

fn setup_alliances() {
  set_alliance(AllPlayers, Player7, Ally);
  set_alliance(Player7, AllPlayers, Ally);
  
  set_alliance(Player7, Player8, Enemy);
  set_alliance(Player8, Player7, Enemy);

  for <PlayerId> in (Player1, Player2, Player3, Player4, Player5, Player6) {
    set_vision(PlayerId, Player7, true);
  }
}

fn spawn_if_present<PlayerId, Unit, Loc>(PlayerId, Unit, Loc) {
  if (is_present(PlayerId)) {
    spawn(Unit, PlayerId, 1, Loc);
    set_doodad(PlayerId, AllUnits, Disable, DisableClick);
  }
}

fn spawn_heroes() {
  for <PlayerId> in (Player1, Player2, Player3, Player4, Player5, Player6) {
    spawn_if_present(PlayerId, HeroJimRaynorMarine, MapCenter);
  }
  
  center_view(AllPlayers, MapCenter);
}

// end initialization stuff

for <TargetLoc> in (ProbeTarget1, ProbeTarget2, ProbeTarget3, ProbeTarget4) {
  bring(Player8, AtLeast, 1, AllUnits, TargetLoc) => {
    order(AllUnits, Player8, Patrol, TargetLoc, MapCenter);
  }

  bring(Player7, AtLeast, 1, ProtossProbe, TargetLoc) => {
    sleep(500);
    order(AllUnits, Player7, Move, TargetLoc, Delivery);
  }
}

bring(Player7, AtLeast, 1, ProtossProbe, Delivery) => {
  kill(ProtossProbe, Player7, 1, Delivery);
}

bring(Player7, AtLeast, 1, PowerupDataDisk, Delivery) => {
  kill(PowerupDataDisk, Player7, 1, Delivery);
  add_score(AllPlayers, Custom, 1);
  add_resource(AllPlayers, Gas, 8);
}

fn spawn_probe() {
  spawn(ProtossProbe, Player7, 1, MapCenter);
  ping(AllPlayers, MapCenter);
  print("A probe has spawned in the center and is heading for a cargo beacon, protect it!");
  sleep(500);

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
