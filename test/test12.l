fn main() {
  print("playing test.wav");
  play_sound("test.wav");
  
  while (true) {
    poll_events();
  }
}
