var stream = require('stream');
var util = require('util');
// use Node.js Writable, otherwise load polyfill
var Writable = stream.Writable ||
  require('readable-stream').Writable;

var memStore = { };

/* Writable memory stream */
function WMStrm(key, options) {
  // allow use without new operator
  if (!(this instanceof WMStrm)) {
    return new WMStrm(key, options);
  }
  Writable.call(this, options); // init super
  this.key = key; // save key
  memStore[key] = new Buffer(''); // empty
}
util.inherits(WMStrm, Writable);

WMStrm.prototype._write = function (chunk, enc, cb) {
  // our memory store stores things in buffers
  var buffer = (Buffer.isBuffer(chunk)) ?
    chunk :  // already is Buffer use it
    new Buffer(chunk, enc);  // string, convert

  // concat to the buffer already there
  memStore[this.key] = Buffer.concat([memStore[this.key], buffer]);
  cb();
};


var bindings = require('bindings')('json-tape.node');


var fs = require('fs');

var input = fs.readFileSync('./broke.json').toString();
var tape = fs.readFileSync('./test2.tape').toString();

// console.log(JSON.parse(input));

var runTest = function(num) {
	console.log('Running test: ' + num);

	var wstream = new WMStrm('foo');

	setInterval(function() {
		console.log('poll');
		console.log('value', memStore.foo.toString());
	}, 1000);

	bindings.play(input, tape, wstream);	

	// wstream.on('finish', function () {
	//   console.log('finished writing');
	//   console.log('value is:', memStore.foo.toString());
	// });

	// wstream.write('hello ');
	// wstream.write('world');
	// wstream.end();
	// var buffer = Buffer.allocUnsafe(100);
	// buffer.fill(0);

	// bindings.play(input, tape, buffer);

	// res.end(buffer);
}

var wait = function(i) {
	console.log('Waiting');
	setTimeout(wait, 1000);
};

wait();

runTest(1);
