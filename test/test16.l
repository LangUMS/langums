countdown(Exactly, 1) => {
  print("3");
  
  if (true) {
    return;
  }
  
  print("4");
}

fn test(q) {
  print("QQ");
  if (q < 5) {
    print("NN");
    return;
  }
  
  print("MM");
  var foo = 5;
  foo++;
}

fn main() {
  print("1");
  test(2);
  
  set_countdown(3);
  
  print("2");
  
  while (true) {
    poll_events();
  }
}
