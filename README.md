## LangUMS

LangUMS is an imperative programming language with C-like syntax for creating custom maps for the game StarCraft: Brood War.

It supercedes the trigger functionality in editors such as SCMDraft 2 and the official StarEdit.
You will still want to use an editor to make the actual map, place locations and units but the triggers are done by LangUMS.

Table of Contents
=================
  * [Usage](#usage)
  * [Language features](#language-features)
  * [Language basics](#language-basics)
  * [Event handlers](#event-handlers)
  * [Built-in functions](#built-in-functions)
  * [Built-in event conditions](#built-in-event-conditions)
  * [Spawning units with properties](#spawning-units-with-properties)
  * [Preprocessor](#preprocessor)
  * [Template functions](#template-functions)
  * [Template event declarations](#template-event-declarations)
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
    * [State](#state)
    * [AllianceStatus](#alliancestatus)
    * [Player](#player)
    * [ScoreType](#scoretype)
    * [LeaderboardType](#leaderboardtype)
    * [UnitProperty](#unitproperty)
    * [Unit](#unit)
    * [AIScript](#aiscript)

### Bugs, issues and feature requests should go to [the issues section](https://github.com/AlexanderDzhoganov/langums/issues).
### I accept and merge [pull requests](https://github.com/AlexanderDzhoganov/langums/pulls) (please follow the code style of the project).
### Guides and tutorials go [in the wiki](https://github.com/AlexanderDzhoganov/langums/wiki).
### [Discord channel](https://discord.gg/TNehfve) for support and discussion.

## Usage

1. Get the latest langums.exe from [here](https://github.com/AlexanderDzhoganov/langums/blob/master/langums.exe?raw=true).

2. You will need to install [Microsoft Visual Studio 2015 Redistributable](https://www.microsoft.com/en-us/download/details.aspx?id=48145) (x86) if you don't have it already.

3. Run langums.exe with your map, source file and destination file e.g.

```
langums.exe --src my_map.scx --lang my_map.l --dst my_map_final.scx
```

4. Now you can run `my_map_final.scx` directly in the game or open it with an editor.

There is [a wonderful extension for VS Code](https://marketplace.visualstudio.com/items?itemName=glenstorm.langums) by Matthew Burton (aka Glenstorm) if you like your code neatly formatted and highlighted. It also has automated code completion.

## Language features

- C-like syntax
- Single primitive type - unsigned 32-bit integer
- Local (block scoped) and global variables
- Static arrays
- Functions with arguments and a return value
- Expressions e.g. `((foo + 42) - bar)`
- Unsigned integer arithmetic with overflow detection
- Postfix increment and decrement operators - `x++`, `y--`
- Arithmetic operators - `+`, `-`, `/`, `*`
- Comparison operators - `<`, `<=`, `==`, `!=`, `>=`, `>`
- Boolean operators - `&&`, `||`
- `if` and `if/else` statements
- `while` loop
- Event handlers

## Language basics

You can try out all of the examples below using the [test.scx map from here](https://github.com/AlexanderDzhoganov/langums/blob/master/test/test.scx?raw=true).

Every program must contain a `main()` function which is the entry point where the code starts executing immediately after the map starts.
The "hello world" program for LangUMS would look like:

```c
fn main() {
  print("Hello World!"); // prints Hello World! for all players
}
```

The example above will print `Hello World!` on every player's screen.

Important notes:

* All statements must end with a semicolon (`;`).
* Block statements (code enclosed in `{}`) do not need a semicolon at the end.
* C-style comments with `//` are supported.

The more complex example below will give 36 minerals total to Player1. It declares a global variable called `foo`, a local variable `bar` and then calls the `add_resource()` built-in within a while loop.
The quantity argument for `add_resource()` is the expression `foo + 2`. Some built-ins support expressions for some of their arguments.

```c
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

```c
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

You can also have static arrays. At the moment arrays can only be indexed with constants.

```c
global my_array[8];

fn main() {
  my_array[Player1] = 42;
  my_array[Player2] = 0x07;
  
  if (my_array[Player2] == 15) {
    print("foo");
  }
  
  var foo[4];
  foo[0] = 10;
  foo[1] = 13 * 2;
}
```

Notes:

* Number literals can be entered as hexadecimals by preceding them with `0x` e.g. `0xB4DF00D`.
* You can index arrays with the [Player](#player) constants.

## Event handlers

Any useful LangUMS program will need to execute code in response to in-game events. The facility for this is called event handlers.

An event handler is somewhat like a function that takes no arguments and returns no values. Instead it specifies one or more conditions separated by commas and a block of code to execute.

Whenever all the specified conditions for the handler are true the code in its body will be executed. At this point it acts like a normal function i.e. you can call
other functions from it, set global variables, call built-ins.

Here is an event handler that executes whenever Player1 brings 5 marines to the location named `BringMarinesHere` and has at least 25 gas:

```c
bring(Player1, Exactly, 5, TerranMarine, "BringMarinesHere"),
accumulate(Player1, AtLeast, 25, Gas) => {
  print("The marines have arrived!");
}
```

Once we have our handlers setup we need to call the built-in function `poll_events()` at regular intervals. The whole program demonstrating the event above would look like:

```c
bring(Player1, Exactly, 5, TerranMarine, "BringMarinesHere"),
accumulate(Player1, AtLeast, 25, Gas) => {
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

```c
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

Notes:

* Arguments named `Expression` can be either numeric constants e.g. `42` or expressions like `x * 3`.
* Text arguments must be passed in `"` quotes e.g. `"This is some text"`.
* Location arguments can be passed without quotes if they do not contain spaces e.g. `MyLocation`, but `"My Location"` needs to be in quotes.
* The special value `AnyLocation` can be passed to `Location` type arguments.

### Unit functions

| Function prototype                                                                               | Description                                          |
|--------------------------------------------------------------------------------------------------|------------------------------------------------------|
| spawn([Unit](#unit), [Player](#player), Expression, Location, optional: Props)                   | Spawns units at a location with optional properties. |
| kill([Unit](#unit), [Player](#player), Expression, optional: Location)                           | Kills units at an optional location.                 |
| remove([Unit](#unit), [Player](#player), Expression, optional: Location)                         | Removes units at an optional location.               |
| move([Unit](#unit), [Player](#player), Expression, SrcLocation, DstLocation)                     | Moves units from one location to another.            |
| order([Unit](#unit), [Player](#player), [Order](#order), SrcLocation, DstLocation)               | Orders a units to move, attack or patrol.            |
| modify([Unit](#unit), [Player](#player), Expression, [UnitMod](#unitmod), ModQuantity, Location) | Modifies a unit's HP, SP, energy or hangar count.    |
| give([Unit](#unit), [SrcPlayer](#player), [DstPlayer](#player), Expression, Location)            | Gives units to another player.                       |
| set_doodad([Player](#player), [Unit](#unit), [State](#state), Location)                          | Sets/ toggles doodad [State](#state).                |
| set_invincibility([Player](#player), [Unit](#unit), [State](#state), Location)                   | Sets/ toggles invincibility for units at location.   |
| run_ai_script([Player](#player), [AIScript](#aiscript), optional: Location)                      | Runs an AI script.                                   |

### UI functions

| Function prototype                                                     | Description                                               |
|------------------------------------------------------------------------|-----------------------------------------------------------|
| center_view([Player](#player), Location)                               | Centers the view on a location for a player.              |
| ping([Player](#player), Location)                                      | Triggers a minimap ping on a location for a player.       |
| talking_portrait([Player](#player), [Unit](#unit), Quantity)           | Shows the unit talking portrait for an amount of seconds. |
| set_vision([SrcPlayer](#player), [DstPlayer](#player), true/ false)    | Sets whether DstPlayer will share vision with SrcPlayer.  |

### Resource functions

| Function prototype                                                          | Description                           |
|-----------------------------------------------------------------------------|---------------------------------------|
| set_resource([Player](#player), [ResourceType](#resourcetype), Expression)  | Sets the resource count for a player. |
| add_resource([Player](#player), [ResourceType](#resourcetype), Expression)  | Gives resources to a player.          |
| take_resource([Player](#player), [ResourceType](#resourcetype), Expression) | Takes resources from a player.        |

### Misc functions

| Function prototype                            | Description                                                                                                                                           |
|-----------------------------------------------|-------------------------------------------------------------------------------------------------------------------------------------------------------|
| poll_events()                                 | Runs any associated event handlers.                                                                                                                   |
| print(Text, optional: [Player](#player))      | Prints a message, defaults to all players.                                                                                                            |
| random()                                      | Returns a random value between 0 and 255 (inclusive).                                                                                                 |
| is_present([Player](#player), ...)            | Checks if a player is in the game. [See here for more info](#how-can-you-tell-if-a-player-is-in-the-game-how-do-you-get-the-total-number-of-players). |
| sleep(Quantity)                               | Waits for a given amount of milliseconds. (Use with care!)                                                                                            |
| pause_game()                                  | Pauses the game (singleplayer only)                                                                                                                   |
| unpause_game()                                | Unpauses the game (singleplayer only)                                                                                                                 |
| set_next_scenario(Text)                       | Sets the next map to run (singleplayer only)                                                                                                          |
| play_sound(Text, optional: [Player](#player)) | Plays a sound from a .wav file. Defaults to all players. [See here for more info](#how-do-you-use-play_sound).                                        |
| clear_buffered_events()                       | Clears all buffered events from the event queue (if there are any).                                                                                   |

### Score functions

| Function prototype                                                              | Description                                                                                              |
|---------------------------------------------------------------------------------|----------------------------------------------------------------------------------------------------------|
| set_score([Player](#player), [ScoreType](#scoretype), Expression)               | Sets the score of a player.                                                                              |
| add_score([Player](#player), [ScoreType](#scoretype), Expression)               | Add to the score of a player.                                                                            |
| subtract_score([Player](#player), [ScoreType](#scoretype), Expression)          | Subtracts from the score of a player.                                                                    |
| set_deaths([Player](#player), [Unit](#unit), Expression)                        | Sets the death count for a unit. (Caution!)                                                              |
| add_deaths([Player](#player), [Unit](#unit), Expression)                        | Adds to the death count for a unit. (Caution!)                                                           |
| remove_deaths([Player](#player), [Unit](#unit), Expression)                     | Subtracts from the death count for a unit. (Caution!)                                                    |
| show_leaderboard(Text, [LeaderboardType](#leaderboardtype), ...)                | Shows the leaderboard. [See here for more info.](#variants-of-the-show_leaderboard-built-in)             |
| show_leaderboard_goal(Text, [LeaderboardType](#leaderboardtype), Quantity, ...) | Shows the leaderboard with a goal. [See here for more info.](#variants-of-the-show_leaderboard-built-in) |
| leaderboard_show_cpu([State](#state))                                           | Shows/ hides computer players from the leaderboard.                                                      |

### Game functions

| Function prototype                                                                          | Description                                                  |
|---------------------------------------------------------------------------------------------|--------------------------------------------------------------|
| end([Player](#player), [EndCondition](#endcondition))                                       | Ends the game for player with [EndCondition](#endcondition). |
| set_alliance([Player](#player), [TargetPlayer](#player), [AllianceStatus](#alliancestatus)) | Sets the alliance status between two players.                |
| set_mission_objectives(Text, optional: [Player](#player))                                   | Sets the mission objectives, defaults to all players.        |
| move_loc([Unit](#unit), [Player](#player), SrcLocation, DstLocation)                        | Centers DstLocation on a unit at SrcLocation.                |
| set_countdown(Expression)                                                                   | Sets the countdown timer.                                    |
| add_countdown(Expression)                                                                   | Adds to the countdown timer.                                 |
| sub_countdown(Expression)                                                                   | Subtracts from the countdown timer.                          |
| pause_countdown()                                                                           | Pauses the countdown timer.                                  |
| unpause_countdown()                                                                         | Unpauses the countdown timer.                                |
| mute_unit_speech()                                                                          | Mutes unit speech.                                           |
| unmute_unit_speech()                                                                        | Unmutes unit speech.                                         |

## Built-in event conditions

### Unit conditions

| Event prototype                                                                        | Description                                                               |
|----------------------------------------------------------------------------------------|---------------------------------------------------------------------------|
| bring([Player](#player), [Comparison](#comparison), Quantity, [Unit](#unit), Location) | When a player owns a quantity of units at location.                       |
| commands([Player](#player), [Comparison](#comparison), Quantity, [Unit](#unit))        | When a player commands a number of units.                                 |
| commands_least([Player](#player), [Unit](#unit), optional: Location)                   | When a player commands the least number of units at an optional location. |
| commands_most([Player](#player), [Unit](#unit), optional: Location)                    | When a player commands the most number of units at an optional location.  |
| killed([Player](#player), [Comparison](#comparison), Quantity, [Unit](#unit))          | When a player has killed a number of units.                               |
| killed_least([Player](#player), [Unit](#unit))                                         | When a player has killed the lowest quantity of a given unit.             |
| killed_most([Player](#player), [Unit](#unit))                                          | When a player has killed the highest quantity of a given unit.            |
| deaths([Player](#player), [Comparison](#comparison), Quantity, [Unit](#unit))          | When a player has lost a number of units.                                 |

### Resource conditions

| Event prototype                                                                                   | Description                                       |
|---------------------------------------------------------------------------------------------------|---------------------------------------------------|
| accumulate([Player](#player), [Comparison](#comparison), Quantity, [ResourceType](#resourcetype)) | When a player owns a quantity of resources.       |
| most_resources([Player](#player), [ResourceType](#resourcetype))                                  | When a player owns the most of a given resource.  |
| least_resources([Player](#player), [ResourceType](#resourcetype))                                 | When a player owns the least of a given resource. |

### Game conditions

| Event prototype                                                   | Description                                                    |
|-------------------------------------------------------------------|----------------------------------------------------------------|
| elapsed_time([Comparison](#comparison), Quantity)                 | When a certain amount of time has elapsed.                     |
| countdown([Comparison](#comparison), Quantity)                    | When the countdown timer reaches a amount of seconds.          |
| opponents([Player](#player), [Comparison](#comparison), Quantity) | When a player has a number of opponents remaining in the game. |

### Score conditions

| Event prototype                                                                        | Description                                     |
|----------------------------------------------------------------------------------------|-------------------------------------------------|
| score([Player](#player), [ScoreType](#scoretype), [Comparison](#comparison), Quantity) | When a player's score reaches a given quantity. |
| lowest_score([Player](#player), [ScoreType](#scoretype))                               | When a player has the lowest score.             |
| highest_score([Player](#player), [ScoreType](#scoretype))                              | When a player has the highest score.            |

## Spawning units with properties

Due to the way map data is structured spawning units with different properties like health and energy is not straightforward. LangUMS offers a flexible way to deal with this issue. Using the `unit` construct you can add up to 64 different unit declarations in your code. A unit declaration that sets the unit's health to 50% is shown below.

```c
unit MyUnitProps {
  Health = 50
}
```

You can mix any properties from the [UnitProperty](#unitproperty) constants. Here is a unit declaration that sets the unit's energy to 0% and makes it invincible.

```c
unit MyInvincibleUnit {
  Energy = 0,
  Invincible = true
}
```

After you have your unit declarations you can use them to spawn units by passing the name as the fifth argument to the `spawn()` built-in e.g.

```c
spawn(TerranMarine, Player1, 1, "TestLocation", MyUnitProps);
```

Below is a full example that spawns a burrowed lurker at 10% health for Player1.

```c
unit LurkerType1 {
  Health = 10,
  Burrowed = true
}

fn main() {
  spawn(ZergLurker, Player1, 1, "TestLocation", LurkerType1);
}
```

## Variants of the show_leaderboard() built-in

The `show_leaderboard()` built-in has several variants that take different arguments. They are listed below.

```c
show_leaderboard("My Leaderboard", Control, Unit);
show_leaderboard("My Leaderboard", ControlAtLocation, Unit, Location);
show_leaderboard("My Leaderboard", Kills, Unit);
show_leaderboard("My Leaderboard", Points, ScoreType);
show_leaderboard("My Leaderboard", Resources, ResourceType);
show_leaderboard("My Leaderboard", Greed);

show_leaderboard_goal("My Leaderboard", Control, Quantity, Unit);
show_leaderboard_goal("My Leaderboard", ControlAtLocation, Quantity, Unit, Location);
show_leaderboard_goal("My Leaderboard", Kills, Quantity, Unit);
show_leaderboard_goal("My Leaderboard", Points, Quantity, ScoreType);
show_leaderboard_goal("My Leaderboard", Resources, Quantity, ResourceType);
show_leaderboard_goal("My Leaderboard", Greed, Quantity);
```

Example usage of all of the above:

```c
show_leaderboard("My Leaderboard", Control, TerranMarine);
show_leaderboard("My Leaderboard", ControlAtLocation, ZergZergling, "MyLocation");
show_leaderboard("My Leaderboard", Kills, ZergHydralisk);
show_leaderboard("My Leaderboard", Points, Buildings);
show_leaderboard("My Leaderboard", Resources, Gas);
show_leaderboard("My Leaderboard", Greed);

show_leaderboard_goal("My Leaderboard", Control, 100, ZergZergling);
show_leaderboard_goal("My Leaderboard", ControlAtLocation, 100, TerranMarine, "MyLocation");
show_leaderboard_goal("My Leaderboard", Kills, 100, ZergHydralisk);
show_leaderboard_goal("My Leaderboard", Points, 100, Units);
show_leaderboard_goal("My Leaderboard", Resources, 100, Minerals);
show_leaderboard_goal("My Leaderboard", Greed, 100);
```

## Preprocessor

The LangUMS compiler features a simple preprocessor that functions similarly to the one in C/ C++. The preprocessor runs before any actual parsing has occured.

- `#define KEY VALUE` will add a new macro definition, all further occurences of `KEY` will be replaced with `VALUE`. You can override previous definitions by calling `#define` again.
- `#undef KEY` will remove an already existing macro definition
- `#include filename` will fetch the contents of `filename` and copy/ paste them at the `#include` point. Note that unlike C the filename is not enclosed in quotes `"`.

## Template functions

Some built-in functions take special kinds of values like player names, unit names or locations. Those can't be stored within LangUMS primitive values. Template functions allow you to "template" one or more of their arguments so you can call them with these special values. Take a look at the example below.

```c
fn spawn_units<T, L>(T, L, qty) {
  talking_portrait(T, 5);
  spawn(T, Player1, qty, L);
  order(T, Player1, Move, L, "TestLocation2");
}

fn main() {
  spawn_units(TerranMarine, "TestLocation", 5);
}
```

`fn spawn_units<T, L>(T, L, qty)` declares a template function called `spawn_units` that takes three arguments `T`, `L` and `qty`. The `<T, L>` part lets the compiler know that the T and L "variables" should be treated in a different way. Later on when `spawn_units(TerranMarine, "TestLocation", 5);` gets called an actual non-templated function will be instantiated and called instead. In this example the instantiated function would look like:

```c
fn spawn_units(qty) {
  talking_portrait(TerranMarine, 5);
  spawn(TerranMarine, Player1, qty, "TestLocation");
  order(TerranMarine, Player1, Move, "TestLocation", "TestLocation2");
}
````

The template argument is gone and all instances of it have been replaced with its value. You can have as many templated arguments on a function as you need. Any built-in function argument that is not of `Expression` type can and should be passed as a template argument.

## Template event declarations

Sometimes you wish to create an event handler for all players but still know exactly which player triggered it. You can of course copy & paste the same event several times and change the player in each one but that would be pretty ugly and unmaintainable. For example say you have a shop in your map and want every human player to be able to use it. You could do it like this:

```c
accumulate(Player1, AtLeast, 10, Minerals),
bring(Player1, AtLeast, 1, AllUnits, BuyMarines) => {
  take_resource(Player1, Minerals, 10);
  spawn(TerranMarine, Player1, 1, BuyMarines);
  print("You purchased a marine, take good care of it.");
}

accumulate(Player2, AtLeast, 10, Minerals),
bring(Player2, AtLeast, 1, AllUnits, BuyMarines) => {
  take_resource(Player2, Minerals, 10);
  spawn(TerranMarine, Player2, 1, BuyMarines);
  print("You purchased a marine, take good care of it.");
}

accumulate(Player3, AtLeast, 10, Minerals),
bring(Player3, AtLeast, 1, AllUnits, BuyMarines) => {
  take_resource(Player3, Minerals, 10);
  spawn(TerranMarine, Player3, 1, BuyMarines);
  print("You purchased a marine, take good care of it.");
}

... etc for all human players in your map ...
```

Or you could use a template event declarations like the example below.

```c
for <PlayerId> in (Player1, Player2, Player3, Player4, Player5, Player6) {
  accumulate(PlayerId, AtLeast, 10, Minerals),
  bring(PlayerId, AtLeast, 1, AllUnits, BuyMarines) => {
    take_resource(PlayerId, Minerals, 10);
    spawn(TerranMarine, PlayerId, 1, BuyMarines);
    print("You purchased a marine, take good care of it.");
  }
```

The result is the same but it helps to not repeat the same code. You can use almost anything as an event template list like players, locations, units, resource types, etc.
At the moment nested template event declarations are not supported but it's a planned feature for the near future.

## Examples

[Look at the examples/ folder here for examples.](https://github.com/AlexanderDzhoganov/langums/tree/master/examples)
Feel free to contribute your own.

## FAQ

#### How does the code generation work?

LangUMS is compiled into a linearized intermediate representation. The IR is then optimized and emitted as trigger chains.
Think of it as a kind of wacky virtual machine.

#### Can you stop the compiler from replacing the existing triggers in the map?

Yes. Use the `--preserve-triggers` option.

#### How do you change which player owns the main triggers?

With the `--triggers-owner` command-line option.

#### How do you change which death counts are used for storage?

Use the `--reg` command-line option to pass a registers file. See `Integrating with existing maps` for more info.

#### Some piece of code behaves weirdly or is clearly executed wrong. What can I do?

Please report it [by adding a new issue here](https://github.com/AlexanderDzhoganov/langums/issues/new).
You can try using the `--disable-optimizations` option, if that fixes the issue it's a bug in the optimizer. In any case please report it.

#### The compiler emits more triggers than I'd like. What can I do?

The biggest culprit for this is the amount of triggers emitted for arithmetic operations.
By default LangUMS is tweaked for values up to 8192. If you don't need such large values in your map you can set the `--copy-batch-size` command-line argument to a lower value e.g. 1024.
This will lower the amount of emitted triggers with the tradeoff that any arithmetic on larger numbers will take more than one cycle.

As a general rule you should set `--copy-batch-size` to the next power of 2 of the largest number you use in your map. If uncertain leave it to the default value.

Multiplication will not work correctly with numbers larger than `--copy-batch-size`.

#### What happens when the main() function returns?

At the moment control returns back to the start of `main()` if for some reason you return from it. In most cases you want to be calling `poll_events()` in an infinite loop inside `main()` so this shouldn't be an issue.

#### How to put text consisting of several lines in the code?

You can add multi-line strings in the code with `"""` e.g.

```
set_mission_objectives("""My multi-line string
This is a second line
This is a third line""");
```

#### How do you use play_sound()?

Put your sounds in .wav format in the same folder as your source .scx map file. For example if you have `my_sound.wav` you can play it with

```
play_sound("my_sound.wav");
```

LangUMS will take care of adding the sound file to the map archive for you.

#### How can you tell if a player is in the game? How do you get the total number of players?

You can call `is_present()` for both cases. To check if a player or players are in game:

```c
if (is_present(Player1, Player2)) {
  print("Player 1 and player 2 are in the game");
}
```

To get the total number of players in the game call `is_present()` with no arguments:

```c
var playerCount = is_present();
```

#### Can you put non-ASCII strings in the Text arguments?

Yes. Make sure to save your code source file as UTF-8 and non-ASCII characters should display properly in the game. It is currently unknown if this applies to patches before 1.18 which added unicode support to the game.

#### Do you plan to rewrite this in Rust?

No, but thanks for asking.

## Limitations

- The player selected with `--triggers-owner` (player 8 by default) must always be in the game (preferably a CPU player). The triggers owner leaving the game leads to undefined behavior. 
- Multiplication only works with numbers up to the value set by `--copy-batch-size` (8192 by default).
- Avoid using huge numbers in general. Additions and subtractions with numbers up to 8192 will always complete in one cycle with the default settings. See the FAQ answer on `--copy-batch-size` for further info.
- Division is implemented suboptimally at the moment and can take many cycles to complete. This will change soon.
- There are about 410 registers available for variables and the stack by default. The variable storage grows upwards and the stack grows downwards. Overflowing either one into the other is undefined behavior. In the future the compiler will probably catch this and refuse to continue. You can use the `--reg` option to provide a registers list that the compiler can use, see `Integrating with existing maps` section.
- You can have up to 238 event handlers, this limitation will be lifted in the future.
- All function calls are inlined due to complexities of implementing the call & ret pair of instructions. This increases code size (number of triggers) quite a bit more than what it would be otherwise. This will probably change in the near future as I explore further options. At the current time avoid really long functions that are called from many places. Recursion of any kind is not allowed.

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
[Here you can find](https://github.com/AlexanderDzhoganov/langums#unit) a list of all unit types.

#### Use set_deaths(), add_deaths() and remove_deaths() built-in functions

You can manipulate your existing death counts with the death count built-ins e.g.

```c
set_deaths(Player5, TerranMarine, foo);
```

will set the death counter for Player5's marines to the value of variable `foo`.

## For project contributors

### Compiling the code

A Visual Studio 2017 project is provided in the [langums/](https://github.com/AlexanderDzhoganov/langums/tree/master/langums) folder which is preconfigured and you just need to run it. However you should be able to get it working with any modern C++ compiler. The code has no external dependencies and is written in portable C++. You will need a compiler with support for `std::experimental::filesystem`. Contributions of a Makefile as well as support for other build systems are welcome.

### Parts of LangUMS

#### Frontend
- [CHK library](https://github.com/AlexanderDzhoganov/langums/tree/master/src/libchk)
- [Preprocessor](https://github.com/AlexanderDzhoganov/langums/blob/master/src/parser/preprocessor.cpp)
- [Parser](https://github.com/AlexanderDzhoganov/langums/blob/master/src/parser/parser.cpp)
- [AST](https://github.com/AlexanderDzhoganov/langums/blob/master/src/ast/ast.h)
- [AST optimizer](https://github.com/AlexanderDzhoganov/langums/blob/master/src/parser/ast_optimizer.cpp)

#### Backend (IR)

- [IR language](https://github.com/AlexanderDzhoganov/langums/blob/master/src/compiler/ir_instructions.h)
- [IR generation](https://github.com/AlexanderDzhoganov/langums/blob/master/src/compiler/ir.cpp)
- [IR optimizer](https://github.com/AlexanderDzhoganov/langums/blob/master/src/compiler/ir_optimizer.cpp)

#### Backend (codegen)

- [Codegen](https://github.com/AlexanderDzhoganov/langums/blob/master/src/compiler/compiler.cpp)
- [TriggerBuilder](https://github.com/AlexanderDzhoganov/langums/blob/master/src/compiler/triggerbuilder.cpp)

## Future plans

- else-if statement
- for loop
- Improved AST/ IR optimization
- Live debugger
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
Minerals (or Ore)
Gas (or Vespene)
Both (or OreAndGas)
```

### Comparison

```
AtLeast (or LessOrEquals)
AtMost (or GreaterOrEquals)
Exactly or (Equals)
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
Shields
Hangar
```

### State

```
Enable
Disable
Toggle
```

### AllianceStatus

```
Ally
Enemy
AlliedVictory
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
Player8
Player9
Player10
Player11
Player12
AllPlayers
NeutralPlayers
Force1
Force2
Force3
Force4
```

### ScoreType

```
Total
Units
Buildings
UnitsAndBuildings
Kills
Razings
KillsAndRazings
Custom
```

### LeaderboardType

```
ControlAtLocation
Control
Greed
Kills
Points
Resources
```

### UnitProperty

```
HitPoints (or Health)
ShieldPoints (or Shields)
Energy
ResourceAmount
HangarCount
Cloaked
Burrowed
InTransit
Hallucinated
Invincible
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

### AIScript

```
JunkYardDog
MoveDarkTemplarsToRegion
ValueThisAreaHigher
EnterClosestBunker
SetGenericCommandTarget
MakeTheseUnitsPatrol
EnterTransport
ExitTransport
AINukeHere
AIHarassHere
CastDisruptionWeb
CastRecall
SendAllUnitsOnStrategicSuicideMissions
SendAllUnitsOnRandomSuicideMissions
TerranCustomLevel
ZergCustomLevel
ProtossCustomLevel
TerranExpansionCustomLevel
ZergExpansionCustomLevel
ProtossExpansionCustomLevel
TerranCampaignEasy
TerranCampaignMedium
TerranCampaignDifficult
TerranCampaignInsane
TerranCampaignAreaTown
ZergCampaignEasy
ZergCampaignMedium
ZergCampaignDifficult
ZergCampaignInsane
ZergCampaignAreaTown
ProtossCampaignEasy
ProtossCampaignMedium
ProtossCampaignDifficult
ProtossCampaignInsane
ProtossCampaignAreaTown
ExpansionTerranCampaignEasy
ExpansionTerranCampaignMedium
ExpansionTerranCampaignDifficult
ExpansionTerranCampaignInsane
ExpansionTerranCampaignAreaTown
ExpansionZergCampaignEasy
ExpansionZergCampaignMedium
ExpansionZergCampaignDifficult
ExpansionZergCampaignInsane
ExpansionZergCampaignAreaTown
ExpansionProtossCampaignEasy
ExpansionProtossCampaignMedium
ExpansionProtossCampaignDifficult
ExpansionProtossCampaignInsane
ExpansionProtossCampaignAreaTown
ClearPreviousCombatData
SetPlayerToEnemyHere
SetPlayerToAllyHere
SwitchComputerPlayerToRescuePassive
TurnOnSharedVisionOfPlayer1WithCurrentPlayer
TurnOnSharedVisionOfPlayer2WithCurrentPlayer
TurnOnSharedVisionOfPlayer3WithCurrentPlayer
TurnOnSharedVisionOfPlayer4WithCurrentPlayer
TurnOnSharedVisionOfPlayer5WithCurrentPlayer
TurnOnSharedVisionOfPlayer6WithCurrentPlayer
TurnOnSharedVisionOfPlayer7WithCurrentPlayer
TurnOnSharedVisionOfPlayer8WithCurrentPlayer
TurnOffSharedVisionOfPlayer1WithCurrentPlayer
TurnOffSharedVisionOfPlayer2WithCurrentPlayer
TurnOffSharedVisionOfPlayer3WithCurrentPlayer
TurnOffSharedVisionOfPlayer4WithCurrentPlayer
TurnOffSharedVisionOfPlayer5WithCurrentPlayer
TurnOffSharedVisionOfPlayer6WithCurrentPlayer
TurnOffSharedVisionOfPlayer7WithCurrentPlayer
TurnOffSharedVisionOfPlayer8WithCurrentPlayer
Terran3ZergTown
Terran5TerranMainTown
Terran5TerranHarvestTown
Terran6AirAttackZerg
Terran6GroundAttackZerg
Terran6ZergSupportTown
Terran7BottomZergTown
Terran7RightZergTown
Terran7MiddleZergTown
Terran8CondeferateTown
Terran9LightAttack
Terran9HeavyAttack
Terran10CondeferateTowns
Terran11ZergTown
Terran11LowerProtossTown
Terran11UpperProtossTown
Terran12NukeTown
Terran12PhoenixTown
Terran12TankTown
Terran1ElectronicDistribution
Terran2ElectronicDistribution
Terran3ElectronicDistribution
Terran1Shareware
Terran2Shareware
Terran3Shareware
Terran4Shareware
Terran5Shareware
Zerg1TerranTown
Zerg2ProtossTown
Zerg3TerranTown
Zerg4RightTerranTown
Zerg4LowerTerranTown
Zerg6ProtossTown
Zerg7AirTown
Zerg7GroundTown
Zerg7SupportTown
Zerg8ScoutTown
Zerg8TemplarTown
Zerg9TealProtoss
Zerg9LeftYellowProtoss
Zerg9RightYellowProtoss
Zerg9LeftOrangeProtoss
Zerg9RightOrangeProtoss
Zerg10LeftTealAttack
Zerg10RightTealSupport
Zerg10LeftYellowSupport
Zerg10RightYellowAttack
Zerg10RedProtoss
Protoss1ZergTown
Protoss2ZergTown
Protoss3AirZergTown
Protoss3GroundZergTown
Protoss4ZergTown
Protoss5ZergTownIsland
Protoss5ZergTownBase
Protoss7LeftProtossTown
Protoss7RightProtossTown
Protoss7ShrineProtoss
Protoss8LeftProtossTown
Protoss8RightProtossTown
Protoss8ProtossDefenders
Protoss9GroundZerg
Protoss9AirZerg
Protoss9SpellZerg
Protoss10MiniTowns
Protoss10MiniTownMaster
Protoss10OvermindDefenders
BroodWarProtoss1TownA
BroodWarProtoss1TownB
BroodWarProtoss1TownC
BroodWarProtoss1TownD
BroodWarProtoss1TownE
BroodWarProtoss1TownF
BroodWarProtoss2TownA
BroodWarProtoss2TownB
BroodWarProtoss2TownC
BroodWarProtoss2TownD
BroodWarProtoss2TownE
BroodWarProtoss2TownF
BroodWarProtoss3TownA
BroodWarProtoss3TownB
BroodWarProtoss3TownC
BroodWarProtoss3TownD
BroodWarProtoss3TownE
BroodWarProtoss3TownF
BroodWarProtoss4TownA
BroodWarProtoss4TownB
BroodWarProtoss4TownC
BroodWarProtoss4TownD
BroodWarProtoss4TownE
BroodWarProtoss4TownF
BroodWarProtoss5TownA
BroodWarProtoss5TownB
BroodWarProtoss5TownC
BroodWarProtoss5TownD
BroodWarProtoss5TownE
BroodWarProtoss5TownF
BroodWarProtoss6TownA
BroodWarProtoss6TownB
BroodWarProtoss6TownC
BroodWarProtoss6TownD
BroodWarProtoss6TownE
BroodWarProtoss6TownF
BroodWarProtoss7TownA
BroodWarProtoss7TownB
BroodWarProtoss7TownC
BroodWarProtoss7TownD
BroodWarProtoss7TownE
BroodWarProtoss7TownF
BroodWarProtoss8TownA
BroodWarProtoss8TownB
BroodWarProtoss8TownC
BroodWarProtoss8TownD
BroodWarProtoss8TownE
BroodWarProtoss8TownF
BroodWarTerran1TownA
BroodWarTerran1TownB
BroodWarTerran1TownC
BroodWarTerran1TownD
BroodWarTerran1TownE
BroodWarTerran1TownF
BroodWarTerran2TownA
BroodWarTerran2TownB
BroodWarTerran2TownC
BroodWarTerran2TownD
BroodWarTerran2TownE
BroodWarTerran2TownF
BroodWarTerran3TownA
BroodWarTerran3TownB
BroodWarTerran3TownC
BroodWarTerran3TownD
BroodWarTerran3TownE
BroodWarTerran3TownF
BroodWarTerran4TownA
BroodWarTerran4TownB
BroodWarTerran4TownC
BroodWarTerran4TownD
BroodWarTerran4TownE
BroodWarTerran4TownF
BroodWarTerran5TownA
BroodWarTerran5TownB
BroodWarTerran5TownC
BroodWarTerran5TownD
BroodWarTerran5TownE
BroodWarTerran5TownF
BroodWarTerran6TownA
BroodWarTerran6TownB
BroodWarTerran6TownC
BroodWarTerran6TownD
BroodWarTerran6TownE
BroodWarTerran6TownF
BroodWarTerran7TownA
BroodWarTerran7TownB
BroodWarTerran7TownC
BroodWarTerran7TownD
BroodWarTerran7TownE
BroodWarTerran7TownF
BroodWarTerran8TownA
BroodWarTerran8TownB
BroodWarTerran8TownC
BroodWarTerran8TownD
BroodWarTerran8TownE
BroodWarTerran8TownF
BroodWarZerg1TownA
BroodWarZerg1TownB
BroodWarZerg1TownC
BroodWarZerg1TownD
BroodWarZerg1TownE
BroodWarZerg1TownF
BroodWarZerg2TownA
BroodWarZerg2TownB
BroodWarZerg2TownC
BroodWarZerg2TownD
BroodWarZerg2TownE
BroodWarZerg2TownF
BroodWarZerg3TownA
BroodWarZerg3TownB
BroodWarZerg3TownC
BroodWarZerg3TownD
BroodWarZerg3TownE
BroodWarZerg3TownF
BroodWarZerg4TownA
BroodWarZerg4TownB
BroodWarZerg4TownC
BroodWarZerg4TownD
BroodWarZerg4TownE
BroodWarZerg4TownF
BroodWarZerg5TownA
BroodWarZerg5TownB
BroodWarZerg5TownC
BroodWarZerg5TownD
BroodWarZerg5TownE
BroodWarZerg5TownF
BroodWarZerg6TownA
BroodWarZerg6TownB
BroodWarZerg6TownC
BroodWarZerg6TownD
BroodWarZerg6TownE
BroodWarZerg6TownF
BroodWarZerg7TownA
BroodWarZerg7TownB
BroodWarZerg7TownC
BroodWarZerg7TownD
BroodWarZerg7TownE
BroodWarZerg7TownF
BroodWarZerg8TownA
BroodWarZerg8TownB
BroodWarZerg8TownC
BroodWarZerg8TownD
BroodWarZerg8TownE
BroodWarZerg8TownF
BroodWarZerg9TownA
BroodWarZerg9TownB
BroodWarZerg9TownC
BroodWarZerg9TownD
BroodWarZerg9TownE
BroodWarZerg9TownF
BroodWarZerg10TownA
BroodWarZerg10TownB
BroodWarZerg10TownC
BroodWarZerg10TownD
BroodWarZerg10TownE
BroodWarZerg10TownF
```
