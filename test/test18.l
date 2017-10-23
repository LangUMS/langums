global count;
global distraction;

value(count, Exactly, 1) => {
	print("Finished countdown!");
}

fn main() {
	count = 50;
	while (true) {
		count--;
		distraction--;
		poll_events();
	}
}
