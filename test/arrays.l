#src test.scx

global player_score[8];

deaths(Player2, AtLeast, 1, TerranMarine) => {
  remove_deaths(Player2, TerranMarine, 1);
  player_score[0]++;
  player_score[1]++;
}

fn main() {
  var foo[4];
  foo[0] = 15;
  foo[1] = 42;
  
  if (foo[0] == 15) {
    print("foo[0] == 15");
  }
  
  if ((foo[1] * 2) == 84) {
    print("(foo[1] * 2) == 84");
  }

  while (true) {
    poll_events();
  }
}
