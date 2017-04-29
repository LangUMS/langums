#define CONSTANT_A 3
#define CONSTANT_B 2
#define CONSTANT_C 5

fn main() {
  print("hello world");
  
  var q = 0;
  if ((q * 5) == 0) {
    print("0. correct result");
  } else {
    print("0. incorrect result");
  }
  
  var x = CONSTANT_A;
  var y = x * CONSTANT_B;
  
  if (y == (CONSTANT_A * CONSTANT_B)) {
    print("1. correct result");
  } else {
    print("1. incorrect result :(");
  }
  
  var z = y * CONSTANT_C;
  
  if (z == (CONSTANT_A * CONSTANT_B * CONSTANT_C)) {
    print("2. correct result");
  } else {
    print("2. incorrect result :(");
  }
  
  while (true) {
    poll_events();
  }
}
