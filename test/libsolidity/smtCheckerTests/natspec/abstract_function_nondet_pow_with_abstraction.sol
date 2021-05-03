contract C {
	uint[] vs;

	uint x;

	constructor(uint _x) {
		require(_x > 0);
		x = _x;
	}

	function e(uint _e) public {
		x = pow(_e, _e);
		x = pow(x, _e);
		x = pow(x, _e);
		x = pow(x, _e);
		vs.push(x % 2);
	}

	function inv() public view {
		if (vs.length == 0)
			return;
		assert(vs[0] < 2);
	}

	/// @custom:smtchecker abstract-function-nondet
    function pow(uint base, uint exponent) internal pure returns (uint) {
        if (base == 0) {
            return 0;
        }
        if (exponent == 0) {
            return 1;
        }
        if (exponent == 1) {
            return base;
        }
        uint y = 1;
        while(exponent > 1) {
            if(exponent % 2 == 0) {
                base = base * base;
                exponent = exponent / 2;
            } else {
                y = base * y;
                base = base * base;
                exponent = (exponent - 1) / 2;
            }
        }
        return base * y;
    }
}
// ====
// SMTEngine: chc
// ----
// Warning 4281: (206-211): CHC: Division by zero happens here.\nCounterexample:\nvs = [], x = 1\n_e = 0\n\nTransaction trace:\nC.constructor(1)\nState: vs = [], x = 1\nC.e(0)
