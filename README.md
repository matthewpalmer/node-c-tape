# node-c-tape

This is a C++ variant of my [json-tape](https://github.com/matthewpalmer/json-tape) library with bindings for Node.js. This lets you replay a set of mutations to an initial JSON state. Take a look at that repo for more information on the instruction format.

This library uses [RapidJSON](https://github.com/miloyip/rapidjson) for JSON parsing and stringify, which is much faster than the native V8 implementation.

It's 6x faster than the equivalent JSON mutation done only in JavaScript and V8.

## How does it work?
If you wanted to mutate some JSON, you’d first have to parse the string to a JSON object, do the mutations, and then re-stringify. This can be pretty slow. There are faster JSON parse and stringify implementations out there—RapidJSON is one of them.

Instead, we pass and receive strings in JavaScript, and leave the JSON operations to the C++ code. This has two benefits

* JSON operations no longer block the main thread
* JSON operations are way faster because they use RapidJSON

## How ready is it?

It works and it can be used, though it's not 100% implemented. This is more of a jumping off point if you happen to have the very niche performance needs that we do.
