#define LUCKY_NUMBER 90

countdown(Exactly, 0) => {
  set_countdown(2 + rnd256() / 80);
  
  if (rnd256() >= LUCKY_NUMBER) {
    print("lucky you");
    spawn(TerranMarine, Player1, 1 + rnd256() / 40, "TestLocation");
    order(TerranMarine, Player1, Attack, "TestLocation", "TestLocation2");
  } else {
    print("try again next time");
    spawn(TerranMarine, Player2, 1 + rnd256() / 40, "TestLocation2");
    order(TerranMarine, Player2, Attack, "TestLocation2", "TestLocation");
  }
}

fn main() {
  while(true) {
    poll_events();
  }
}
