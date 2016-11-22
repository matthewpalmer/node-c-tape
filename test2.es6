var bindings = require('bindings')('json-tape.node');



var fs = require('fs');

var input = fs.readFileSync('./broke.json').toString();
var tape = fs.readFileSync('./test2.tape').toString();

// console.log(JSON.parse(input));

var runTest = function(num) {
	console.log('Running test: ' + num);
	const buffer = bindings.play(input, tape, (res) => {
		console.log('Finished test: ' + num +'. Result length: ' + res.length);
	});

	// bindings.play(input, tape, (res) => {
	// 	console.log('Finished test: ' + num +'. Result length: ' + res.length);

	// 	// console.log(res);
	// 	const json = JSON.parse(res);
	// 	// console.log(json.results);
	// 	// console.log(json.results.map(r => r.title).length)
	// });

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
// runTest(2);
// runTest(3);
