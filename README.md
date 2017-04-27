## LangUMS

LangUMS is a procedural imperative programming language with C-like syntax for creating custom maps for the game StarCraft: BroodWar.

It supercedes the trigger functionality offered by editors such as SCMDraft 2 and the official Blizzard one.
You still need an editor to make the actual map, preplace locations and units, etc. but the triggers are added by LangUMS.

Table of Contents
=================
  * [Usage](#usage)
  * [Language features](#language-features)
  * [Language basics](#language-basics)
  * [Event handlers](#event-handlers)
  * [Built-in functions](#built-in-functions)
  * [Built-in event conditions](#built-in-event-conditions)
  * [Preprocessor](#preprocessor)
  * [Examples](#examples)
  * [FAQ](#faq)
  * [Limitations](#limitations)
  * [Integration with existing maps](#integration-with-existing-maps)
  * [For project contributors](#for-project-contributors)
  * [Future plans](#future-plans)
  * [Built-in constants](#built-in-constants)
    * [EndCondition](#endcondition)
    * [ResourceType](#resourcetype)
    * [Comparison](#comparison)
    * [Order](#order)
    * [UnitMod](#unitmod)
    * [Player](#player)
    * [Unit](#unit)

### LangUMS is early work-in-progress. Code contributions (and any other contributions) are welcome and will be credited.

### Bugs, issues and feature requests should go to [the issues section](https://github.com/AlexanderDzhoganov/langums/issues).
### I accept and merge [pull requests](https://github.com/AlexanderDzhoganov/langums/pulls) (please follow the code style of the project).
### Guides and tutorials go [in the wiki](https://github.com/AlexanderDzhoganov/langums/wiki).
### [Discord channel](https://discord.gg/BcY23) for support and discussion.

## Usage

1. Get the latest langums.exe from [here](https://github.com/AlexanderDzhoganov/langums/blob/master/langums.exe?raw=true).

2. You will need to install [Microsoft Visual Studio 2015 Redistributable](https://www.microsoft.com/en-us/download/details.aspx?id=48145) (x86) if you don't have it already.

3. Run langums.exe with your map, source file and destination file e.g.

```
langums.exe --src my_map.scx --lang my_map.l --dst my_map_final.scx
```

4. Now you can run `my_map_final.scx` directly in the game or open it with an editor.

## Language features

- C-like syntax
- Single primitive type - unsigned 32-bit int
- Local (block scoped) and global variables
- Functions with arguments and a return value
- Expressions with stack-based evaluation e.g. `((foo + 42) - bar)`
- Unsigned integer arithmetic with overflow detection
- Postfix increment and decrement operators - `x++`, `y--`
- Arithmetic operators - `+`, `-`, `/`, `*`
- Comparison operators - `<`, `<=`, `==`, `!=`, `>=`, `>`
- `if` and `if/else` statements
- `while` loop
- Event handlers

## Language basics

You can try out all of the examples below using the [test.scx map from here](https://github.com/AlexanderDzhoganov/langums/blob/master/test/test.scx?raw=true).

Every program must contain a `main()` function which is the entry point where the code starts executing immediately after the map starts.
The "hello world" program for LangUMS would look like:

```
fn main() {
  print("Hello World!"); // prints Hello World! for Player 1
}
```

Which will print `Hello World!` on Player1's screen.

Note: All statements must end with a semicolon (`;`). Block statements (code enclosed in `{}`) do not need a semicolon at the end. C-style comments with `//` are supported.

The more complex example below will give 36 minerals total to Player1. It declares a global variable called `foo`, a local variable `bar` and then calls the `add_resource()` built-in within a while loop.
The quantity argument for `add_resource()` is the expression `foo + 2`. Some built-ins support expressions for some of their arguments.

```
global foo = 10;

fn main() {
  var bar = 3;

  while(bar--) {
    add_resource(Player1, Minerals, foo + 2);
    foo = foo + 10;
  }
}
```

You can define your own functions that accept arguments and return a value.

```
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
```

## Event handlers

Any useful LangUMS program will need to execute code in response to in-game events. The facility for this is called event handlers.

An event handler is somewhat like a function that takes no arguments and returns no values. Instead it specifies one or more conditions and a block of code to execute.

Whenever all the specified conditions become true the code for the handler will be executed. At this point the event handler acts more or less like a normal function i.e. you can call
other functions from it, set global variables, call built-ins.

Here is an event handler that executes whenever Player1 brings 5 marines to the location named `BringMarinesHere`:

```
bring(Player1, Exactly, 5, TerranMarine, "BringMarinesHere") => {
  print("The marines have arrived!");
}
```

Once we have our handlers setup we need to call the built-in function `poll_events()` at regular intervals. The whole program demonstrating the event above would look like:
```
bring(Player1, Exactly, 5, TerranMarine, "BringMarinesHere") => {
  print("The marines have arrived!");
}

fn main() {
  while(true) {
    poll_events();
  }
}
```

You can do other kinds of processing between the `poll_events()` calls and you can be sure that no event handlers will be interleaved with your program's execution. Events are buffered
so if you don't call `poll_events()` for a long time it will fire off all buffered events one after another the next time it's called.

A slighly more contrived example of events. Also demonstrates usage of the preprocessor `#define` directive.

```
#define MAX_SWAPS 3

global allowedSwaps = MAX_SWAPS;

bring(Player1, AtLeast, 1, TerranMarine, "TestLocation2"),
elapsed_time(AtLeast, 15) => {
  if (allowedSwaps == 0) {
    print("Sorry, you have no more swaps left.");
  } else { 
    allowedSwaps--;
    kill(TerranMarine, Player1, 1, "TestLocation2");
    spawn(ProtossZealot, Player1, 1, "TestLocation");
    
    print("Here is your zealot.");
  }
}

bring(Player1, AtLeast, 1, TerranMarine, "TestLocation2"),
elapsed_time(AtMost, 15) => {
  print("You have to wait 15 seconds before being able to swap.");
}

bring(Player1, AtLeast, 1, ProtossZealot, "TestLocation2") => {
  if (allowedSwaps == 0) {
    print("Sorry, you have no more swaps left.");
  } else {
    allowedSwaps--;
    kill(ProtossZealot, Player1, 1, "TestLocation2");
    spawn(TerranMarine, Player1, 1, "TestLocation");
    
    print("Here is your marine.");
  }
}

fn main() {
  spawn(TerranMarine, Player1, 1, "TestLocation");
  
  while (true) {
    poll_events();
  }
}
```

## Built-in functions

Note: Arguments named `QuantityExpression` can be either numeric constants e.g. `42` or expressions like `x * 3`.

| Function prototype                                                     | Description                                              |
|------------------------------------------------------------------------|----------------------------------------------------------|
| `poll_events()`                                                        | Runs any associated event handlers.                      |
| `print(String, optional: Player)`                                      | Prints a message to a player, defaults to Player1.       |
| `rnd256()`                                                             | Returns a random value between 0 and 255 (inclusive).    |
| `set_resource(Player, ResourceType, QuantityExpression)`               | Sets the resource count for player.                      |
| `add_resource(Player, ResourceType, QuantityExpression)`               | Gives resources to a player.                             |
| `take_resource(Player, ResourceType, QuantityExpression)`              | Takes resources from a player.                           |
| `center_view(Player, Location)`                                        | Centers the view on a location for a player.             |
| `ping(Player, Location)`                                               | Triggers a minimap ping on a location for a player.      |
| `spawn(Unit, Player, QuantityExpression, Location)`                    | Spawns units at a location.                              |
| `kill(Unit, Player, QuantityExpression, optional: Location)`           | Kills units at an optional location.                     |
| `remove(Unit, Player, QuantityExpression, optional: Location)`         | Removes units at an optional location.                   |
| `move(Unit, Player, QuantityExpression, SrcLocation, DstLocation)`     | Moves units from one location to another.                |
| `order(Unit, Player, Order, SrcLocation, DstLocation)`                 | Orders a unit to move, attack or patrol.                 |
| `modify(Unit, Player, QuantityExpression, UnitMod, Percent, Location)` | Modifies a unit's HP, SP, energy or hangar count.        |
| `give(Unit, SrcPlayer, DstPlayer, QuantityExpression, Location)`       | Gives units to another player.                           |
| `move_loc(Unit, Player, SrcLocation, DstLocation)`                     | Centers DstLocation on a unit at SrcLocation.            |
| `end(Player, EndCondition)`                                            | Ends the game for Player with EndCondition.              |
| `set_countdown(Expression)`                                            | Sets the countdown timer.                                |
| `set_deaths(Player, Unit, QuantityExpression)`                         | Sets the death count for a unit. (Caution!)              |
| `add_deaths(Player, Unit, QuantityExpression)`                         | Adds to the death count for a unit. (Caution!)           |
| `remove_deaths(Player, Unit, QuantityExpression)`                      | Subtracts from the death count for a unit. (Caution!)    |
| `talking_portrait(Unit, Seconds)`                                      | Shows the unit talking portrait for an amount of time.   |
| `sleep(Milliseconds)`                                                  | Waits for a specific amount of time. (Dangerous!)        |
| More to be added ...                                                   |                                                          |

## Built-in event conditions

| Event prototype                                                 | Description                                           |
|-----------------------------------------------------------------|-------------------------------------------------------|
| `bring(Player, Comparison, Quantity, Unit, Location)`           | When a player owns a quantity of units at location.   |
| `accumulate(Player, Comparison, Quantity, ResourceType)`        | When a player owns a quantity of resources.           |
| `elapsed_time(Comparison, Quantity)`                            | When a certain amount of time has elapsed.            |
| `commands(Player, Comparison, Quantity, Unit)`                  | When a player commands a number of units.             |
| `killed(Player, Comparison, Quantity, Unit)`                    | When a player has killed a number of units.           |
| `deaths(Player, Comparison, Quantity, Unit)`                    | When a player has lost a number of units.             |
| `countdown(Comparison, Time)`                                   | When the countdown timer reaches a specific time.     |
| More to be added ...                                            |                                                       |

## Preprocessor

The LangUMS compiler features a simple preprocessor that functions similarly to the one in C/ C++. The preprocessor runs before any actual parsing has occured.

- `#define KEY VALUE` will add a new macro definition, all further occurences of `KEY` will be replaced with `VALUE`. You can override previous definitions by calling `#define` again.
- `#undef KEY` will remove an already existing macro definition
- `#include filename` will fetch the contents of `filename` and copy/ paste them at the `#include` point. Note that unlike C the filename is not enclosed in quotes `"`.

## Examples

[Look at the test/ folder here for examples.](https://github.com/AlexanderDzhoganov/langums/tree/master/test)
Feel free to contribute your own.

## FAQ

#### How does the code generation work?

LangUMS is compiled into an intermediate representation (IR) which is then optimized and emitted as trigger chains.
It's a kind of wacky virtual machine.

#### Code documentation?

Will happen sometime in the future. You can help.

#### Can you stop the compiler from replacing the existing triggers in the map?

Yes. Use the `--preserve-triggers` option.

#### How do you change which player owns the main triggers?

With the `--triggers-owner` command-line option.

#### How do you change which player's death counts are used for storage?

Use the `--registers-owner` command-line option.

#### The compiler emits way more triggers than I'd like. What can I do?

The biggest culprit for this is the amount of triggers emitted for arithmetic operations.
By default LangUMS is tweaked for values up to 65535, but if you don't need such large values in your map you can set the `--copy-batch-size` command-line argument to a lower value e.g. 1024.
This will drastically lower the amount of emitted triggers with the tradeoff that any arithmetic on larger numbers will take more than one cycle. In any case you should set the value to the
next power of 2 of the largest number you use in your map. If uncertain leave it to the default value.

#### What happens when the main() function returns?

At the moment control returns back to the start of `main()` if for some reason you return from it. In most cases you want to be calling `poll_events()` in an infinite loop inside `main()` so this shouldn't be an issue.

#### Do you plan to rewrite this in Rust?

No, but thanks for asking.

## Limitations

- One player (out of the 8 possible) must be reserved for LangUMS (unless you are using the `--reg` option). By default player 8 is used for this. This player's stuff needs to remain untouched for LangUMS to do its work. Spawning units for this player will lead to undefined behavior. You can use all other players freely. Preplaced units for this player are also not allowed. You can use the `--registers-owner` option to select the reserved player.
- There are about 240 registers available for variables and the stack by default. The variable storage grows upwards and the stack grows downwards. Overflowing either one into the other is undefined behavior. In the future the compiler will probably catch this and refuse to continue. You can use the `--reg` option to provide a registers list that the compiler can use, see `Integrating with existing maps` section.
- Functions are limited to a maximum of 8 arguments, this limitation can be lifted on request.
- Currently you have can have up to 240 event handlers, this limitation will be lifted in the future.
- Recursion of any kind is not allowed and leads to undefined behavior.
- Multiplication and division can take many cycles to complete, especially with very large numbers.
- In general avoid using huge numbers. Additions and subtractions with numbers up to 65536 will always complete in one cycle with the default settings. See the FAQ answer on `--copy-batch-size` for further info.

## Integration with existing maps

There are some facilities for integrating LangUMS code with existing maps and more will be added in the future.

#### Use --preserve-triggers

The `--preserve-triggers` option makes it so that the compiler doesn't wipe all existing triggers in the map but appends to them instead.

#### Use a registers map file

You can pass a file that tells the compiler which death counts for which players are free to use as general purpose storage.
The format of this file is:

```
Player1, TerranMarine
Player2, TerranGhost
Player4, TerranVulture
Player8, TerranGoliath
```

Where each line contains two items - a player name and a unit name. Note that the order of the lines does not matter.

You can pass this file to the compiler with the `--reg` option e.g.

```
langums.exe --src my_map.scx --lang my_map.l --dst my_map_final.scx --reg my_registers.txt
```

A sample file with the default mappings [is available here](https://github.com/AlexanderDzhoganov/langums/blob/master/registermap.txt?raw=true).

#### Use set_deaths(), add_deaths() and remove_deaths() built-in functions

You can manipulate your existing death counters with the death counter built-in functions e.g.

```
set_deaths(Player5, TerranMarine, foo);
```

Will set the death counter for Player5's marines to the value of variable `foo`.

## For project contributors

### Compiling the code

A Visual Studio 2015 project is provided in the [langums/](https://github.com/AlexanderDzhoganov/langums/tree/master/langums) folder which is preconfigured and you just need to run it. However you should be able to get it working with any modern C++ compiler. The code has no external dependencies and is written in portable C++. You will need a compiler with support for `std::experimental::filesystem`. Contributions of a Makefile as well as support for other build systems are welcome.

### Parts of LangUMS

#### Frontend
- [CHK library](https://github.com/AlexanderDzhoganov/langums/tree/master/src/libchk)
- [Preprocessor](https://github.com/AlexanderDzhoganov/langums/blob/master/src/parser/preprocessor.cpp)
- [Parser](https://github.com/AlexanderDzhoganov/langums/blob/master/src/parser/parser.cpp)
- [AST](https://github.com/AlexanderDzhoganov/langums/blob/master/src/ast/ast.h)
- [AST optimizer](https://github.com/AlexanderDzhoganov/langums/blob/master/src/parser/ast_optimizer.cpp)

#### Backend (IR)

- [IR generation](https://github.com/AlexanderDzhoganov/langums/blob/master/src/compiler/ir.cpp)
- [IR optimizer](https://github.com/AlexanderDzhoganov/langums/blob/master/src/compiler/ir_optimizer.cpp)

#### Backend (codegen)

- [Codegen](https://github.com/AlexanderDzhoganov/langums/blob/master/src/compiler/compiler.cpp)
- [TriggerBuilder](https://github.com/AlexanderDzhoganov/langums/blob/master/src/compiler/triggerbuilder.cpp)

## Future plans

- else-if statement
- for loop
- Arrays
- Template-like metaprogramming facilities
- Improved AST/ IR optimization
- Multiple execution threads

## Built-in constants

### EndCondition

```
Victory
Defeat
Draw
```

### ResourceType

```
Minerals
Gas
```

### Comparison

```
AtLeast
AtMost
Exactly
```

### Order

```
Move
Attack
Patrol
```

### UnitMod

```
Health,
Energy,
Shields,
Hangar
```

### Player

```
Player1
Player2
Player3
Player4
Player5
Player6
Player7
Player8 (Do not use!)
Player9
Player10
Player11
Player12
```

### Unit

```
TerranMarine
TerranGhost
TerranVulture
TerranGoliath
TerranGoliathTurret
TerranSiegeTankTankMode
TerranSiegeTankTankModeTurret
TerranSCV
TerranWraith
TerranScienceVessel
HeroGuiMontag
TerranDropship
TerranBattlecruiser
TerranVultureSpiderMine
TerranNuclearMissile
TerranCivilian
HeroSarahKerrigan
HeroAlanSchezar
HeroAlanSchezarTurret
HeroJimRaynorVulture
HeroJimRaynorMarine
HeroTomKazansky
HeroMagellan
HeroEdmundDukeTankMode
HeroEdmundDukeTankModeTurret
HeroEdmundDukeSiegeMode
HeroEdmundDukeSiegeModeTurret
HeroArcturusMengsk
HeroHyperion
HeroNoradII
TerranSiegeTankSiegeMode
TerranSiegeTankSiegeModeTurret
TerranFirebat
SpellScannerSweep
TerranMedic
ZergLarva
ZergEgg
ZergZergling
ZergHydralisk
ZergUltralisk
ZergBroodling
ZergDrone
ZergOverlord
ZergMutalisk
ZergGuardian
ZergQueen
ZergDefiler
ZergScourge
HeroTorrasque
HeroMatriarch
ZergInfestedTerran
HeroInfestedKerrigan
HeroUncleanOne
HeroHunterKiller
HeroDevouringOne
HeroKukulzaMutalisk
HeroKukulzaGuardian
HeroYggdrasill
TerranValkyrie
ZergCocoon
ProtossCorsair
ProtossDarkTemplar
ZergDevourer
ProtossDarkArchon
ProtossProbe
ProtossZealot
ProtossDragoon
ProtossHighTemplar
ProtossArchon
ProtossShuttle
ProtossScout
ProtossArbiter
ProtossCarrier
ProtossInterceptor
HeroDarkTemplar
HeroZeratul
HeroTassadarZeratulArchon
HeroFenixZealot
HeroFenixDragoon
HeroTassadar
HeroMojo
HeroWarbringer
HeroGantrithor
ProtossReaver
ProtossObserver
ProtossScarab
HeroDanimoth
HeroAldaris
HeroArtanis
CritterRhynadon
CritterBengalaas
SpecialCargoShip
SpecialMercenaryGunship
CritterScantid
CritterKakaru
CritterRagnasaur
CritterUrsadon
ZergLurkerEgg
HeroRaszagal
HeroSamirDuran
HeroAlexeiStukov
SpecialMapRevealer
HeroGerardDuGalle
ZergLurker
HeroInfestedDuran
SpellDisruptionWeb
TerranCommandCenter
TerranComsatStation
TerranNuclearSilo
TerranSupplyDepot
TerranRefinery
TerranBarracks
TerranAcademy
TerranFactory
TerranStarport
TerranControlTower
TerranScienceFacility
TerranCovertOps
TerranPhysicsLab
UnusedTerran1
TerranMachineShop
UnusedTerran2
TerranEngineeringBay
TerranArmory
TerranMissileTurret
TerranBunker
SpecialCrashedNoradII
SpecialIonCannon
PowerupUrajCrystal
PowerupKhalisCrystal
ZergInfestedCommandCenter
ZergHatchery
ZergLair
ZergHive
ZergNydusCanal
ZergHydraliskDen
ZergDefilerMound
ZergGreaterSpire
ZergQueensNest
ZergEvolutionChamber
ZergUltraliskCavern
ZergSpire
ZergSpawningPool
ZergCreepColony
ZergSporeColony
UnusedZerg1
ZergSunkenColony
SpecialOvermindWithShell
SpecialOvermind
ZergExtractor
SpecialMatureChrysalis
SpecialCerebrate
SpecialCerebrateDaggoth
UnusedZerg2
ProtossNexus
ProtossRoboticsFacility
ProtossPylon
ProtossAssimilator
UnusedProtoss1
ProtossObservatory
ProtossGateway
UnusedProtoss2
ProtossPhotonCannon
ProtossCitadelofAdun
ProtossCyberneticsCore
ProtossTemplarArchives
ProtossForge
ProtossStargate
SpecialStasisCellPrison
ProtossFleetBeacon
ProtossArbiterTribunal
ProtossRoboticsSupportBay
ProtossShieldBattery
SpecialKhaydarinCrystalForm
SpecialProtossTemple
SpecialXelNagaTemple
ResourceMineralField
ResourceMineralFieldType2
ResourceMineralFieldType3
UnusedCave
UnusedCaveIn
UnusedCantina
UnusedMiningPlatform
UnusedIndependantCommandCenter
SpecialIndependantStarport
UnusedIndependantJumpGate
UnusedRuins
UnusedKhaydarinCrystalFormation
ResourceVespeneGeyser
SpecialWarpGate
SpecialPsiDisrupter
UnusedZergMarker
UnusedTerranMarker
UnusedProtossMarker
SpecialZergBeacon
SpecialTerranBeacon
SpecialProtossBeacon
SpecialZergFlagBeacon
SpecialTerranFlagBeacon
SpecialProtossFlagBeacon
SpecialPowerGenerator
SpecialOvermindCocoon
SpellDarkSwarm
SpecialFloorMissileTrap
SpecialFloorHatch
SpecialUpperLevelDoor
SpecialRightUpperLevelDoor
SpecialPitDoor
SpecialRightPitDoor
SpecialFloorGunTrap
SpecialWallMissileTrap
SpecialWallFlameTrap
SpecialRightWallMissileTrap
SpecialRightWallFlameTrap
SpecialStartLocation
PowerupFlag
PowerupYoungChrysalis
PowerupPsiEmitter
PowerupDataDisk
PowerupKhaydarinCrystal
PowerupMineralClusterType1
PowerupMineralClusterType2
PowerupProtossGasOrbType1
PowerupProtossGasOrbType2
PowerupZergGasSacType1
PowerupZergGasSacType2
PowerupTerranGasTankType1
PowerupTerranGasTankType2
None
AllUnits
Men
Buildings
Factories
```
