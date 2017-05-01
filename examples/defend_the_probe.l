// Defend the Probe
// map file: defend_the_probe.scx

#define DEBUG false
#define FOUR_SECONDS 4000
#define INVALID_TARGET 7

#define PROBE_COUNT 8

// prices
#define ZERGLING_COST 25
#define MARINE_COST 50
#define ZEALOT_COST 100
//

// globals
global current_wave = 2;
global probe_spawned = false;
//

fn main() {
  setup_alliances();

  center_view(AllPlayers, HeroesSpawn);
  spawn_heroes();

  set_mission_objectives("""- Don't let the probes die.'
- You can buy units at the beacons.
- Upgrades are in the top right corner.
  """);

  if (!DEBUG)
  {
    sleep(FOUR_SECONDS);
    print("Buy units from the beacons.");
    ping(AllPlayers, BuyMarines);
    sleep(FOUR_SECONDS);
    ping(AllPlayers, Upgrades);
    center_view(AllPlayers, Upgrades);
    print("Buy upgrades from here.");
    sleep(FOUR_SECONDS);
  }

  center_view(AllPlayers, HeroesSpawn);
  ping(AllPlayers, HeroesSpawn);
  spawn(ProtossProbe, Player7, PROBE_COUNT, MapCenter);
  print("The probes have spawned, protect them with your life!");
  sleep(2000);
  
  add_resource(AllPlayers, Minerals, 10);
  
  set_countdown(35);
  clear_buffered_events();
  probe_spawned = true;
  order(ProtossProbe, Player7, Move, AnyLocation, MapCenter);

  while (true) {
    move_loc(ProtossProbe, Player7, AnyLocation, ProbeLocation); // center ProbeLocation on any probe
    modify(AllUnits, Force1, 10, Health, 100, "HealStation");
    poll_events();
  }
}

// lose conditions

commands(Player7, Exactly, 0, ProtossProbe) => {
  if (!probe_spawned) {
    return;
  }

  print("The probe is dead. You are losers.");
  end(AllPlayers, Defeat);
}

for <PlayerId> in (Player1, Player2, Player3, Player4, Player5, Player6) {
  commands(PlayerId, Exactly, 0, HeroJimRaynorMarine) => {
    if (!probe_spawned) {
      return;
    }

    print("You have died a glorious death. o7");
    end(PlayerId, Defeat);
  }
}

// end lose conditions

// shop logic

for <PlayerId> in (Player1, Player2, Player3, Player4, Player5, Player6) {
  accumulate(PlayerId, AtLeast, MARINE_COST, Minerals),
  bring(PlayerId, AtLeast, 1, HeroJimRaynorMarine, BuyMarines) => {
    take_resource(PlayerId, Minerals, MARINE_COST);
    spawn(TerranMarine, PlayerId, 1, MapCenter);
  }

  accumulate(PlayerId, AtLeast, ZERGLING_COST, Minerals),
  bring(PlayerId, AtLeast, 1, HeroJimRaynorMarine, BuyZerglings) => {
    take_resource(PlayerId, Minerals, ZERGLING_COST);
    spawn(ZergZergling, PlayerId, 1, MapCenter);
  }

  accumulate(PlayerId, AtLeast, ZEALOT_COST, Minerals),
  bring(PlayerId, AtLeast, 1, HeroJimRaynorMarine, BuyZealots) => {
    take_resource(PlayerId, Minerals, ZEALOT_COST);
    spawn(ProtossZealot, PlayerId, 1, MapCenter);
  }
}

// end shop logic

// reward logic

deaths(Player8, AtLeast, 1, ZergZergling) => {
  remove_deaths(Player8, ZergZergling, 1);
  add_resource(AllPlayers, Minerals, 1);
}

deaths(Player8, AtLeast, 1, ZergHydralisk) => {
  remove_deaths(Player8, ZergHydralisk, 1);
  add_resource(AllPlayers, Minerals, 1);
}

deaths(Player8, AtLeast, 1, ZergUltralisk) => {
  remove_deaths(Player8, ZergUltralisk, 1);
  add_resource(AllPlayers, Minerals, 3);
}

deaths(Player8, AtLeast, 1, ZergInfestedTerran) => {
  remove_deaths(Player8, ZergInfestedTerran, 1);
  add_resource(AllPlayers, Minerals, 3);
}

// end reward logic

fn spawn_next_wave() {
  var x = current_wave + current_wave;
  add_resource(AllPlayers, Gas, x);

  spawn(ZergZergling, Player8, x, EnemySpawn1);
  spawn(ZergZergling, Player8, x, EnemySpawn2);
  
  if (current_wave <= 4) {
    return;
  }

  spawn(ZergHydralisk, Player8, x, EnemySpawn1);
  
  if (current_wave <= 9) {
    return;
  }

  spawn(ZergUltralisk, Player8, x - 8, EnemySpawn2);

  if (current_wave <= 15) {
    return;
  }
  
  spawn(ZergInfestedTerran, Player8, x, EnemySpawn2);
}

fn attack_at<Loc>(Loc) { // order all enemies to attack at a location
  order(AllUnits, Player8, Attack, AnyLocation, Loc);
}

countdown(Exactly, 34) => {
  set_countdown(33);

  current_wave++;
  spawn_next_wave();
  
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
  set_countdown(40);
}

fn spawn_if_present<Player, Unit, Loc>(Player, Unit, Loc) {
  if (is_present(Player)) {
    spawn(Unit, Player, 1, Loc);
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

bring(Player7, AtLeast, 1, ProtossProbe, MapCenter) => {
  pick_random_target(MapCenter);
}

bring(Player7, AtLeast, 1, ProtossProbe, ProbeTarget1) => {
  pick_random_target(ProbeTarget1);
}

bring(Player7, AtLeast, 1, ProtossProbe, ProbeTarget2) => {
  pick_random_target(ProbeTarget2);
}

bring(Player7, AtLeast, 1, ProtossProbe, ProbeTarget3) => {
  pick_random_target(ProbeTarget3);
}

bring(Player7, AtLeast, 1, ProtossProbe, ProbeTarget4) => {
  pick_random_target(ProbeTarget4);
}

bring(Player7, AtLeast, 1, ProtossProbe, ProbeTarget5) => {
  pick_random_target(ProbeTarget5);
}

fn pick_random_target<SrcLoc>(SrcLoc) {
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
}

// end probe random walk
