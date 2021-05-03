contract C {
	/// @custom:smtchecker abstract-function-nondet
	function f(uint x) internal pure returns (uint) {
		return x;
	}
	function g(uint y) public pure {
		uint z = f(y);
		// Generally holds, but here it doesn't because function
		// `f` has been abstracted by nondeterministic values.
		assert(z == y);
	}
}
// ----
// Warning 6328: (297-311): CHC: Assertion violation happens here.\nCounterexample:\n\ny = 0\n\nTransaction trace:\nC.constructor()\nC.g(0)
