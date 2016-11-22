var bindings = require('bindings')('json-tape.node');



var fs = require('fs');

var input = fs.readFileSync('./broke.json').toString();
var tape = fs.readFileSync('./test2.tape').toString();

// console.log(JSON.parse(input));

var runTest = function(num) {
	console.log('Running test: ' + num);

	var buffer = Buffer.allocUnsafe(100);
	buffer.fill(0);

	bindings.play(input, tape, buffer);

	// res.end(buffer);
}

var wait = function(i) {
	console.log('Waiting');
	setTimeout(wait, 1000);
};

wait();

runTest(1);
