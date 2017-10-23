fn test() {
	for <PlayerId> in (Player1, Player2, Player3) {
		print("hello world", PlayerId);
	}

	for <PlayerId, Msg> in (Player1, Player2, Player3), ("foo", "bar", "baz") {
		print(Msg, PlayerId);
	}
}

fn main() {
	test();
}
