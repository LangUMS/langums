#define MAX_SWAPS 3

global allowedSwaps = MAX_SWAPS;

bring(AllPlayers, AtLeast, 1, TerranMarine, "TestLocation2") => {
  if (allowedSwaps == 0) {
    print("Sorry, you have no more swaps left.");
    return;
  }
  
  allowedSwaps--;
  kill(TerranMarine, Player1, 1, "TestLocation2");
  spawn(ProtossZealot, Player1, 1, "TestLocation");
  
  print("Here is your zealot.");
}

bring(AllPlayers, AtLeast, 1, ProtossZealot, "TestLocation2") => {
  if (allowedSwaps == 0) {
    print("Sorry, you have no more swaps left.");
    return;
  }
  
  allowedSwaps--;
  kill(ProtossZealot, Player1, 1, "TestLocation2");
  spawn(TerranMarine, Player1, 1, "TestLocation");
  
  print("Here is your marine.");
}

fn main() {
  spawn(TerranMarine, Player1, 1, "TestLocation");
  
  while (true) {
    poll_events();
  }
}
