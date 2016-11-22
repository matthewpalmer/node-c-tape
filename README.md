# node-c-tape

This is a Node.js C++ variant of [json-tape](https://github.com/matthewpalmer/json-tape), which lets you replay a log of instructions to an initial JSON state. Take a look at that repo for more information on the instruction format.

This library uses [RapidJSON](https://github.com/miloyip/rapidjson) for JSON parsing and stringify, which is much faster than the native V8 implementation.

In our testing we've found that it's roughly 6x faster than the equivalent JSON mutation done only in JavaScript.

## How does it work?
If you wanted to mutate some JSON, you’d first have to parse the string to a JSON object, do the mutations, and then re-stringify. This can be pretty slow. There are faster JSON parse and stringify implementations out there—which is why we use RapidJSON.

Instead, we operate on strings from JavaScript, and leave the JSON operations to the C++ code. This has two benefits

* JSON operations no longer block the main thread
* JSON operations are way faster because they use RapidJSON

## How ready is it?

It works and it can be used, though it's not 100% implemented. This is more of a jumping off point if you happen to have the very niche performance needs that we do.

## Also

I don’t really know C++—sorry about that.
