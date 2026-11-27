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

  // A proxy whose properties are functions that do nothing.
  function sink() {
    if (typeof Proxy === "function") {
      return new Proxy({}, { get: function () { return noop; } });
    }
    return {};
  }

  // Generic shell functions.
  missing("print", noop);
  missing("gc", noop);
  missing("quit", noop);
  missing("load", noop);
  missing("readline", function () { return ""; });
  missing("version", zero);

  // test262: assert.
  function assert() {}
  assert.sameValue = assert.notSameValue = assert.compareArray = noop;
  assert.deepEqual = assert.throwsAsync = noop;
  assert.throws = function (E, fn) { run(fn); };
  g.assert = assert;
  function Test262Error(message) { this.message = message || ""; }
  Test262Error.thrower = noop;
  g.Test262Error = Test262Error;
  g.$DONOTEVALUATE = g.$DONE = noop;
  g.compareArray = function () { return true; };
  g.asyncTest = function (fn) {
    var p = run(fn);
    try {
      if (p && typeof p.then === "function") p.then(noop, noop);
    } catch (e) {}
  };
  var names = ["verifyProperty", "verifyEqualTo", "verifyWritable",
    "verifyNotWritable", "verifyEnumerable", "verifyNotEnumerable",
    "verifyConfigurable", "verifyNotConfigurable"];
  var i;
  for (i = 0; i < names.length; i++) g[names[i]] = noop;

  // test262: typed arrays.
  var taNames = ["Int8Array", "Uint8Array", "Uint8ClampedArray", "Int16Array",
    "Uint16Array", "Int32Array", "Uint32Array", "Float32Array", "Float64Array"];
  var bigNames = ["BigInt64Array", "BigUint64Array"];
  function eachCtor(cb, list) {
    var j;
    for (j = 0; j < list.length; j++) {
      try {
        if (typeof list[j] === "string") {
          if (typeof g[list[j]] === "function") cb(g[list[j]]);
        } else {
          cb(list[j]);
        }
      } catch (e) {}
    }
  }
  g.testWithTypedArrayConstructors = function (cb, list) {
    eachCtor(cb, list || taNames.concat(bigNames));
  };
  g.testWithBigIntTypedArrayConstructors = function (cb) {
    eachCtor(cb, bigNames);
  };

  // test262: $262.
  function make262() {
    var o = {
      global: g,
      gc: typeof g.gc === "function" ? g.gc : noop,
      agent: { start: noop, broadcast: noop, getReport: noop, sleep: noop,
        monotonicNow: zero },
      detachArrayBuffer: function (ab) {
        try { if (ab && typeof ab.transfer === "function") ab.transfer(); }
        catch (e) {}
      },
      evalScript: function (src) { run(String(src)); },
      createRealm: function () {
        try {
          if (typeof g.createRealm === "function") return g.createRealm();
        } catch (e) {}
        return make262();
      }
    };
    return o;
  }
  g.$262 = make262();

  // mjsunit.
  var asserts = ["assertEquals", "assertNotEquals", "assertSame",
    "assertNotSame", "assertTrue", "assertFalse", "assertNull",
    "assertNotNull", "assertEqualsDelta", "assertArrayEquals",
    "assertPropertiesEqual", "assertInstanceof", "assertUnreachable",
    "assertContains", "assertMatches", "assertOptimized",
    "assertUnoptimized", "fail"];
  for (i = 0; i < asserts.length; i++) g[asserts[i]] = noop;
  g.assertThrows = g.assertThrowsEquals = g.assertDoesNotThrow =
    g.assertThrowsAsync = g.assertPromiseResult = g.assertEarlyError =
    function (code) { run(code); };
  g.MjsUnitAssertionError = function (message) { this.message = message; };
  g.__v8native = function () { return noop; };

  // d8 globals.
  missing("Realm", sink());
  missing("d8", sink());
  missing("version", zero);
  missing("performance", { now: function () { return 0; } });
  missing("setTimeout", function (fn) {
    if (typeof Promise === "function") {
      Promise.resolve().then(function () { run(fn); });
    } else {
      run(fn);
    }
  });

  // JSTests.
  g.$vm = sink();
  var jsNames = ["noInline", "noDFG", "noFTL", "noOSRExitFuzzing", "OSRExit",
    "fullGC", "edenGC", "ensureArrayStorage", "transferArrayBuffer",
    "drainMicrotasks", "shouldBe"];
  for (i = 0; i < jsNames.length; i++) g[jsNames[i]] = noop;
  g.numberOfDFGCompiles = zero;
  g.isFinalTier = function () { return false; };
  g.shouldThrow = g.shouldNotThrow = function (fn) { run(fn); };
  g.testLoopCount = 10000;
})(Function("return this")());
