fn main() {
  print("playing test.wav");
  play_wav("test.wav");
  
  while (true) {
    poll_events();
  }
}
