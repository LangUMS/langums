### LangUMS is early work-in-progress. Code contributions (and any other contributions) are welcome and will be credited.

### Bugs, issues and feature requests should go to [the issues section](https://github.com/AlexanderDzhoganov/langums/issues).
### I accept and merge [pull requests](https://github.com/AlexanderDzhoganov/langums/pulls) (please follow the code style of the project).
### Guides and tutorials go [in the wiki](https://github.com/AlexanderDzhoganov/langums/wiki).
### [Discord channel](https://discord.gg/BcY23) for support and discussion.

## LangUMS

LangUMS is a procedural imperative programming language with C-like syntax for creating custom maps for the game StarCraft: BroodWar.

It supercedes the trigger functionality offered by editors such as SCMDraft 2 and the official Blizzard one.
You still need an editor to make the actual map, preplace locations and units, etc, but the triggers are added by LangUMS.

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
- `if` statement
- `while` loop
- Event handlers

## Language basics

Every program must contain a `main()` function which is the entry point where the code starts executing immediately after the map starts.
The "hello world" program for LangUMS would look like:

```
fn main() {
  print("Hello World!");
}
```

Which will print `Hello World!` on Player1's screen.

Note: All statements must end with a semicolon (`;`). Block statements (code enclosed in `{}`) do not need a semicolon at the end.

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

Once we have our handlers setup we need to call a special built-in function called `poll_events()` at regular intervals. The whole program demonstrating the event above would looke like:
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
so if you don't call `poll_events()` for a long time then call it, it will fire off all buffered events one after another the next cycle.

## Built-in functions

| Function prototype                                                 | Description                                              |
|--------------------------------------------------------------------|----------------------------------------------------------|
| `poll_events()`                                                    | Runs any associated event handlers.                      |
| `print(String, optional: Player)`                                  | Prints a message to a player, defaults to Player1.       |
| `set_resource(Player, ResourceType, QuantityExpression)`           | Sets the resource count for player.                      |
| `add_resource(Player, ResourceType, QuantityExpression)`           | Gives resources to a player.                             |
| `take_resource(Player, ResourceType, QuantityExpression)`          | Takes resources from a player.                           |
| `center_view(Player, Location)`                                    | Centers the view on a location for a player.             |
| `ping(Player, Location)`                                           | Triggers a minimap ping on a location for a player.      |
| `spawn(Unit, Player, QuantityExpression, Location)`                | Spawns units at a location.                              |
| `kill(Unit, Player, QuantityExpression, optional: Location)`       | Kills units at an optional location.                     |
| `move(Unit, Player, QuantityExpression, SrcLocation, DstLocation)` | Moves units from one location to another.                |
| `move_loc(Unit, Player, SrcLocation, DstLocation)`                 | Centers DstLocation on a unit at SrcLocation.            |
| `end(Player, EndCondition)`                                        | Ends the game for Player with EndCondition.              |
| `set_countdown(Expression)`                                        | Sets the countdown timer.                                |
| `sleep(Quantity)`                                                  | Sleeps for milliseconds. (Dangerous!)                    |
| More to be added ...                                               |                                                          |

## Built-in event conditions

| Event prototype                                                 | Description                                           |
|-----------------------------------------------------------------|-------------------------------------------------------|
| `bring(Player, Comparison, Quantity, Unit, Location)`           | When a player owns a quantity of units at location.   |
| `accumulate(Player, Comparison, Quantity, ResourceType)`        | When a player owns a quantity of resources.           |
| `elapsed_time(Comparison, Quantity)`                            | When a certain amount of time has elapsed.            |
| `commands(Player, Comparison, Quantity, Unit)`                  | When a player commands a number of units.             |
| `killed(Player, Comparison, Quantity, Unit)`                    | When a player has killed a number of units.           |
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

Yes. Use the `--preserve-triggers` option. Make sure your existing triggers don't mess with Player 8's stuff.

#### The compiler emits way more triggers than I'd like. What can I do?

The biggest culprit for this is the amount of triggers emitted for arithmetic operations.
By default LangUMS is tweaked for values up to 65535, but if you don't need such large values in your map you can set the `--copy-batch-size` command-line argument to a lower value e.g. 1024.
This will drastically lower the amount of emitted triggers with the tradeoff that any arithmetic on larger numbers will take more than one cycle. In any case you should set the value to the
next power of 2 of the largest number you use in your map. If uncertain leave it to the default value.

#### Do you plan to rewrite this in Rust?

No, but thanks for asking.

## Limitations

- Player 8 is reserved and must be a Computer player. This is enforced by the compiler. Also Player 8's stuff needs to remain untouched for LangUMS to do its work. Doing anything with Player 8 e.g. spawning units will lead to undefined behavior. Don't. You can use all other players freely. Preplaced units for Player 8 are also not allowed.
- There are about 240 registers available for variables and the stack. The variable storage grows upwards and the stack grows downwards. Overflowing either one into the other is undefined behavior. In the future the compiler will probably catch this and refuse to continue.
- Functions are limited to a maximum of 8 arguments, this limitation can be lifted on request.
- Currently you have can have up to 253 event handlers, this limitation will be lifted in the future.
- Recursion of any kind is not allowed and leads to undefined behavior.
- Multiplication and division can take many cycles to complete, especially with very large numbers.
- In general avoid using huge numbers. Additions and subtractions with numbers up to 65536 will always complete in one cycle with the default settings. See the FAQ answer on `--copy-batch-size` for further info.

## For programmers

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

- Else/ Else-If statements
- For loop
- Arrays
- Template-like metaprogramming facilities
- Improved AST/ IR optimization
- Multiple execution threads

## Built-in constants

### EndCondition

```
- Victory
- Defeat
- Draw
```

### ResourceType

```
- Minerals
- Gas
```

### Comparison

```
- AtLeast
- AtMost
- Exactly
```

### Player

```
- Player1
- Player2
- Player3
- Player4
- Player5
- Player6
- Player7
- Player8 (Do not use!)
- Player9
- Player10
- Player11
- Player12
```

### Unit

```
- TerranMarine
- TerranGhost
- TerranVulture
- TerranGoliath
- TerranGoliathTurret
- TerranSiegeTankTankMode
- TerranSiegeTankTankModeTurret
- TerranSCV
- TerranWraith
- TerranScienceVessel
- HeroGuiMontag
- TerranDropship
- TerranBattlecruiser
- TerranVultureSpiderMine
- TerranNuclearMissile
- TerranCivilian
- HeroSarahKerrigan
- HeroAlanSchezar
- HeroAlanSchezarTurret
- HeroJimRaynorVulture
- HeroJimRaynorMarine
- HeroTomKazansky
- HeroMagellan
- HeroEdmundDukeTankMode
- HeroEdmundDukeTankModeTurret
- HeroEdmundDukeSiegeMode
- HeroEdmundDukeSiegeModeTurret
- HeroArcturusMengsk
- HeroHyperion
- HeroNoradII
- TerranSiegeTankSiegeMode
- TerranSiegeTankSiegeModeTurret
- TerranFirebat
- SpellScannerSweep
- TerranMedic
- ZergLarva
- ZergEgg
- ZergZergling
- ZergHydralisk
- ZergUltralisk
- ZergBroodling
- ZergDrone
- ZergOverlord
- ZergMutalisk
- ZergGuardian
- ZergQueen
- ZergDefiler
- ZergScourge
- HeroTorrasque
- HeroMatriarch
- ZergInfestedTerran
- HeroInfestedKerrigan
- HeroUncleanOne
- HeroHunterKiller
- HeroDevouringOne
- HeroKukulzaMutalisk
- HeroKukulzaGuardian
- HeroYggdrasill
- TerranValkyrie
- ZergCocoon
- ProtossCorsair
- ProtossDarkTemplar
- ZergDevourer
- ProtossDarkArchon
- ProtossProbe
- ProtossZealot
- ProtossDragoon
- ProtossHighTemplar
- ProtossArchon
- ProtossShuttle
- ProtossScout
- ProtossArbiter
- ProtossCarrier
- ProtossInterceptor
- HeroDarkTemplar
- HeroZeratul
- HeroTassadarZeratulArchon
- HeroFenixZealot
- HeroFenixDragoon
- HeroTassadar
- HeroMojo
- HeroWarbringer
- HeroGantrithor
- ProtossReaver
- ProtossObserver
- ProtossScarab
- HeroDanimoth
- HeroAldaris
- HeroArtanis
- CritterRhynadon
- CritterBengalaas
- SpecialCargoShip
- SpecialMercenaryGunship
- CritterScantid
- CritterKakaru
- CritterRagnasaur
- CritterUrsadon
- ZergLurkerEgg
- HeroRaszagal
- HeroSamirDuran
- HeroAlexeiStukov
- SpecialMapRevealer
- HeroGerardDuGalle
- ZergLurker
- HeroInfestedDuran
- SpellDisruptionWeb
- TerranCommandCenter
- TerranComsatStation
- TerranNuclearSilo
- TerranSupplyDepot
- TerranRefinery
- TerranBarracks
- TerranAcademy
- TerranFactory
- TerranStarport
- TerranControlTower
- TerranScienceFacility
- TerranCovertOps
- TerranPhysicsLab
- UnusedTerran1
- TerranMachineShop
- UnusedTerran2
- TerranEngineeringBay
- TerranArmory
- TerranMissileTurret
- TerranBunker
- SpecialCrashedNoradII
- SpecialIonCannon
- PowerupUrajCrystal
- PowerupKhalisCrystal
- ZergInfestedCommandCenter
- ZergHatchery
- ZergLair
- ZergHive
- ZergNydusCanal
- ZergHydraliskDen
- ZergDefilerMound
- ZergGreaterSpire
- ZergQueensNest
- ZergEvolutionChamber
- ZergUltraliskCavern
- ZergSpire
- ZergSpawningPool
- ZergCreepColony
- ZergSporeColony
- UnusedZerg1
- ZergSunkenColony
- SpecialOvermindWithShell
- SpecialOvermind
- ZergExtractor
- SpecialMatureChrysalis
- SpecialCerebrate
- SpecialCerebrateDaggoth
- UnusedZerg2
- ProtossNexus
- ProtossRoboticsFacility
- ProtossPylon
- ProtossAssimilator
- UnusedProtoss1
- ProtossObservatory
- ProtossGateway
- UnusedProtoss2
- ProtossPhotonCannon
- ProtossCitadelofAdun
- ProtossCyberneticsCore
- ProtossTemplarArchives
- ProtossForge
- ProtossStargate
- SpecialStasisCellPrison
- ProtossFleetBeacon
- ProtossArbiterTribunal
- ProtossRoboticsSupportBay
- ProtossShieldBattery
- SpecialKhaydarinCrystalForm
- SpecialProtossTemple
- SpecialXelNagaTemple
- ResourceMineralField
- ResourceMineralFieldType2
- ResourceMineralFieldType3
- UnusedCave
- UnusedCaveIn
- UnusedCantina
- UnusedMiningPlatform
- UnusedIndependantCommandCenter
- SpecialIndependantStarport
- UnusedIndependantJumpGate
- UnusedRuins
- UnusedKhaydarinCrystalFormation
- ResourceVespeneGeyser
- SpecialWarpGate
- SpecialPsiDisrupter
- UnusedZergMarker
- UnusedTerranMarker
- UnusedProtossMarker
- SpecialZergBeacon
- SpecialTerranBeacon
- SpecialProtossBeacon
- SpecialZergFlagBeacon
- SpecialTerranFlagBeacon
- SpecialProtossFlagBeacon
- SpecialPowerGenerator
- SpecialOvermindCocoon
- SpellDarkSwarm
- SpecialFloorMissileTrap
- SpecialFloorHatch
- SpecialUpperLevelDoor
- SpecialRightUpperLevelDoor
- SpecialPitDoor
- SpecialRightPitDoor
- SpecialFloorGunTrap
- SpecialWallMissileTrap
- SpecialWallFlameTrap
- SpecialRightWallMissileTrap
- SpecialRightWallFlameTrap
- SpecialStartLocation
- PowerupFlag
- PowerupYoungChrysalis
- PowerupPsiEmitter
- PowerupDataDisk
- PowerupKhaydarinCrystal
- PowerupMineralClusterType1
- PowerupMineralClusterType2
- PowerupProtossGasOrbType1
- PowerupProtossGasOrbType2
- PowerupZergGasSacType1
- PowerupZergGasSacType2
- PowerupTerranGasTankType1
- PowerupTerranGasTankType2
- None
- AllUnits
- Men
- Buildings
- Factories
```