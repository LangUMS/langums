fn main() {
  print("hello world");
  
  if (is_present(Player1)) {
    print("Player 1 is here.");
  }
  
  if (is_present(Player6)) {
    print("Player 6 is here.");
  }
  
  if (is_present(Player1, Player2)) {
    print("Players 1 and 2 are here.");
  }
  
  var playerCount = is_present();
  
  while (true) {
    poll_events();
  }
}
