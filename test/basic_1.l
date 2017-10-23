#src test.scx

bring(Player1, AtMost, 0, TerranMarine, "TestLocation") => {
  remove(TerranMarine, Player1, 1);
  spawn(TerranMarine, Player1, 1, "TestLocation", InvincibleUnit);
}

fn main() {
  while(true) {
    poll_events();
  }
}

unit InvincibleUnit {
  Invincible = true
}
