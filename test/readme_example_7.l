#src test.scx

for <PlayerId> in (Player1, Player2, Player3, Player4, Player5, Player6) {
  accumulate(PlayerId, AtLeast, 10, Minerals),
  bring(PlayerId, AtLeast, 1, AllUnits, TestLocation) => {
    take_resource(PlayerId, Minerals, 10);
    spawn(TerranMarine, PlayerId, 1, TestLocation);
    print("You purchased a marine, take good care of it.");
  }
}

fn main() {} 