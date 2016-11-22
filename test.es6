var bindings = require('bindings')('json-tape.node');



var fs = require('fs');

var input = fs.readFileSync('./large.json').toString();
var tape = fs.readFileSync('./test.tape').toString();

var runTest = function(num) {
	console.log('Running test: ' + num);
	bindings.play(input, tape, (res) => {
		console.log('Finished test: ' + num +'. Result length: ' + res.length);

		const json = JSON.parse(res);
		console.log(json);
	});

	// setTimeout(function() {
	// 	runTest(num + 1);
	// }, 550);
}

var wait = function(i) {
	console.log('Waiting');
	setTimeout(wait, 1000);
};

wait();

runTest(1);
