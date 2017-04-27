fn main() {
  var foo = rnd256();
  
  if (foo > 16) {
    print("lucky you");
  }
  
  if (foo <= 64) {
    print("try again next time");
  }
}
