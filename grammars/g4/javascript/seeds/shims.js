//
// american fuzzy lop++ - grammar mutator
// --------------------------------------
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at:
//
//   http://www.apache.org/licenses/LICENSE-2.0
//
// Shims for the test files of test262, V8 mjsunit, and WebKit JSTests.
// The engine runs this file first and then one seed, in one global context.
// Each shim never throws. Each shim that takes code still runs the code.
// This file always replaces the assert functions of the suites. It defines
// every other name only if the engine has no value, so jsc and d8 keep
// their own functions.
// The code uses ES5 syntax only and declares nothing at the top level.
(function (g) {
  function noop() {}
  function zero() { return 0; }

  // Run a function, or a string of code, and ignore every exception.
  function run(code) {
    try {
      if (typeof code === "function") return code();
      if (typeof code === "string") return (0, eval)(code);
    } catch (e) {}
  }

  // Set a global only if the engine has no value for it.
  function missing(name, value) {
    if (typeof g[name] === "undefined") g[name] = value;
  }

  // Set each name of a list to a function that does nothing, if missing.
  function missingNoops(list) {
    for (var k = 0; k < list.length; k++) missing(list[k], noop);
  }

  // A proxy whose properties are functions that do nothing.
  function sink() {
    try {
      if (typeof Proxy === "function") {
        return new Proxy({}, { get: function () { return noop; } });
      }
    } catch (e) {}
    return {};
  }

  // Attach handlers to a promise, or to the result of a function.
  function settle(p, ok, bad) {
    try {
      if (typeof p === "function") p = p();
      if (p && typeof p.then === "function") {
        p.then(function (v) { run(function () { return ok && ok(v); }); },
          function (e) { run(function () { return bad && bad(e); }); });
      }
    } catch (e) {}
  }

  // SameValue without an exception.
  function same(a, b) {
    if (a === b) return a !== 0 || 1 / a === 1 / b;
    return a !== a && b !== b;
  }

  function toStr(v) {
    try { return String(v); } catch (e) { return "[object]"; }
  }

  // Generic shell functions.
  missing("print", noop);
  missing("gc", noop);
  missing("quit", noop);
  missing("load", noop);
  missing("readline", function () { return ""; });
  missing("version", zero);
  missing("clearTimeout", noop);

  // test262: assert.
  function assert() {}
  assert.sameValue = assert.notSameValue = assert.deepEqual = noop;
  assert.compareArray = noop;
  assert.throws = function (E, fn) { run(fn); };
  assert.throwsAsync = function (E, fn) { settle(fn); };
  assert._isSameValue = same;
  assert._toString = toStr;
  g.assert = assert;
  function Test262Error(message) {
    if (!(this instanceof Test262Error)) return new Test262Error(message);
    this.message = message || "";
  }
  Test262Error.thrower = noop;
  missing("Test262Error", Test262Error);
  missingNoops(["$DONOTEVALUATE", "$DONE", "verifyProperty", "verifyEqualTo",
    "verifyWritable", "verifyNotWritable", "verifyEnumerable",
    "verifyNotEnumerable", "verifyConfigurable", "verifyNotConfigurable"]);
  function compareArray() { return true; }
  compareArray.format = function (a) { return "[" + toStr(a) + "]"; };
  missing("compareArray", compareArray);
  missing("asyncTest", function (fn) { settle(fn); });

  // test262: typed arrays.
  var taNames = ["Int8Array", "Uint8Array", "Uint8ClampedArray", "Int16Array",
    "Uint16Array", "Int32Array", "Uint32Array", "Float32Array", "Float64Array"];
  var bigNames = ["BigInt64Array", "BigUint64Array"];
  function eachCtor(cb, list) {
    try {
      for (var j = 0; j < list.length; j++) {
        try {
          if (typeof list[j] === "string") {
            if (typeof g[list[j]] === "function") cb(g[list[j]]);
          } else {
            cb(list[j]);
          }
        } catch (e) {}
      }
    } catch (e) {}
  }
  missing("testWithTypedArrayConstructors", function (cb, list) {
    eachCtor(cb, list || taNames.concat(bigNames));
  });
  missing("testWithBigIntTypedArrayConstructors", function (cb) {
    eachCtor(cb, bigNames);
  });

  // test262: $262.
  function make262() {
    return {
      global: g,
      gc: typeof g.gc === "function" ? g.gc : noop,
      agent: { start: noop, broadcast: noop, getReport: noop, sleep: noop,
        monotonicNow: zero },
      detachArrayBuffer: function (ab) {
        try { if (ab && typeof ab.transfer === "function") ab.transfer(); }
        catch (e) {}
      },
      evalScript: function (src) {
        try { run(String(src)); } catch (e) {}
      },
      createRealm: function () {
        try {
          if (typeof g.createRealm === "function") return g.createRealm();
        } catch (e) {}
        return make262();
      }
    };
  }
  missing("$262", make262());

  // mjsunit.
  var asserts = ["assertEquals", "assertNotEquals", "assertSame",
    "assertNotSame", "assertTrue", "assertFalse", "assertNull",
    "assertNotNull", "assertEqualsDelta", "assertArrayEquals",
    "assertPropertiesEqual", "assertInstanceof", "assertUnreachable",
    "assertContains", "assertMatches", "assertOptimized",
    "assertUnoptimized", "fail", "shouldBe"];
  for (var i = 0; i < asserts.length; i++) g[asserts[i]] = noop;
  g.assertThrows = g.assertThrowsEquals = g.assertDoesNotThrow =
    g.assertEarlyError = g.shouldThrow = g.shouldNotThrow =
    function (code) { run(code); };
  g.assertThrowsAsync = function (p) { settle(p); };
  g.assertPromiseResult = settle;
  function MjsUnitAssertionError(message) {
    if (!(this instanceof MjsUnitAssertionError)) {
      return new MjsUnitAssertionError(message);
    }
    this.message = message;
  }
  missing("MjsUnitAssertionError", MjsUnitAssertionError);
  missing("__v8native", function () { return noop; });

  // d8 globals.
  missing("Realm", sink());
  missing("d8", sink());
  missing("performance", { now: function () { return 0; } });
  missing("setTimeout", function (fn) {
    try {
      if (typeof Promise === "function") {
        Promise.resolve().then(function () { run(fn); });
        return 0;
      }
    } catch (e) {}
    run(fn);
    return 0;
  });

  // JSTests.
  missing("$vm", sink());
  missingNoops(["noInline", "noDFG", "noFTL", "noOSRExitFuzzing", "OSRExit",
    "fullGC", "edenGC", "ensureArrayStorage", "transferArrayBuffer",
    "drainMicrotasks"]);
  missing("numberOfDFGCompiles", zero);
  missing("isFinalTier", function () { return false; });
  missing("testLoopCount", 10000);
})(Function("return this")());
