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
// The engine runs this file and then one seed, in one global context.
// A shim never throws, and it runs the code that it gets. The asserts are
// always replaced. Other names are set only if missing, so jsc and d8 keep
// their own functions. ES5 syntax only; nothing is declared at top level.
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

  // A function proxy. A call does nothing. Each property that `base` does
  // not have gives the proxy again, so `d8.file.execute()` works.
  function sink(base) {
    try {
      if (typeof Proxy === "function") {
        var p = new Proxy(function () { "use strict"; }, {
          get: function (t, k) { return base && k in base ? base[k] : p; }
        });
        return p;
      }
    } catch (e) {}
    return base || {};
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

  // An error type that also works without `new`.
  function errorType() {
    function E(message) {
      if (!(this instanceof E)) return new E(message);
      this.message = message || "";
    }
    return E;
  }

  function toStr(v) {
    try { return String(v); } catch (e) { return "[object]"; }
  }

  // A new global object if the engine can make one, else this global.
  function newGlobal() {
    try {
      if (typeof g.createRealm === "function") return g.createRealm();
    } catch (e) {}
    return g;
  }

  // Run code in the global object `o`. Return the value of the code.
  function evalIn(o, code) {
    try { return o === g ? (0, eval)(String(code)) : o.eval(String(code)); }
    catch (e) {}
  }

  // Generic shell functions.
  missingNoops(["print", "gc", "quit", "load", "clearTimeout"]);
  missing("readline", function () { return ""; });
  missing("version", zero);
  missing("console", sink());

  // test262: assert.
  function assert() {}
  assert.sameValue = assert.notSameValue = assert.deepEqual = noop;
  assert.compareArray = noop;
  assert.throws = function (E, fn) { run(fn); };
  assert.throwsAsync = function (E, fn) { settle(fn); };
  assert._isSameValue = same;
  assert._toString = toStr;
  g.assert = assert;
  var Test262Error = errorType();
  Test262Error.thrower = noop;
  missing("Test262Error", Test262Error);
  missingNoops(["$DONOTEVALUATE", "$DONE", "verifyProperty", "verifyEqualTo",
    "verifyWritable", "verifyNotWritable", "verifyEnumerable",
    "verifyNotEnumerable", "verifyConfigurable", "verifyNotConfigurable"]);
  function compareArray() { return true; }
  compareArray.format = function (a) { return "[" + toStr(a) + "]"; };
  missing("compareArray", compareArray);
  missing("asyncTest", function (fn) { settle(fn); });
  missing("isConstructor", function (f) {
    try { Reflect.construct(Object, [], f); return true; } catch (e) {}
    return false;
  });
  missing("fnGlobalObject", function () { return g; });
  missing("decimalToPercentHexString", function (n) {
    try { return "%" + ("0" + n.toString(16).toUpperCase()).slice(-2); }
    catch (e) {}
  });
  missing("testLenientAndStrict", function (code) {
    run(code);
    if (typeof code === "string") run("'use strict';\n" + code);
  });
  missing("allowProxyTraps", Object);
  missing("$MAX_ITERATIONS", 100000);
  missing("TemporalHelpers", sink());

  // test262: typed arrays.
  var floatNames = ["Float16Array", "Float32Array", "Float64Array"];
  var taNames = ["Int8Array", "Uint8Array", "Uint8ClampedArray", "Int16Array",
    "Uint16Array", "Int32Array", "Uint32Array"].concat(floatNames);
  var bigNames = ["BigInt64Array", "BigUint64Array"];
  var allNames = taNames.concat(bigNames);
  // Call `cb` with each function of a list. A string names a property of
  // `from` (default: the global object).
  function eachCtor(cb, list, from) {
    try {
      for (var j = 0; j < list.length; j++) {
        try {
          var c = typeof list[j] === "string" ? (from || g)[list[j]] : list[j];
          if (typeof c === "function") cb(c);
        } catch (e) {}
      }
    } catch (e) {}
  }
  function ctorList(names) {
    var r = [];
    eachCtor(function (c) { r.push(c); }, names);
    return r;
  }
  missing("testWithTypedArrayConstructors", function (cb, list) {
    eachCtor(cb, list || allNames);
  });
  missing("testWithBigIntTypedArrayConstructors", function (cb) {
    eachCtor(cb, bigNames);
  });
  missing("ctors", ctorList(allNames));
  missing("anyTypedArrayConstructors", ctorList(allNames));
  missing("floatArrayConstructors", ctorList(floatNames));
  missing("typedArrayConstructors", ctorList(taNames));
  // A resizable buffer, if the engine has them.
  function rab(len, max) {
    try { return new ArrayBuffer(len, { maxByteLength: max }); } catch (e) {}
  }
  missing("CreateResizableArrayBuffer", rab);
  missing("CreateRabForTest", function (C) {
    try { return rab(4 * C.BYTES_PER_ELEMENT, 8 * C.BYTES_PER_ELEMENT); }
    catch (e) {}
  });
  missing("MayNeedBigInt", function (ta, n) {
    try {
      if (/Big/.test(Object.prototype.toString.call(ta))) return BigInt(n);
    } catch (e) {}
    return n;
  });
  missing("ToNumbers", function (a) {
    try { return [].map.call(a, Number); } catch (e) { return []; }
  });
  missing("CollectValuesAndResize", function (n, values, buf, after, to) {
    try { if (values.push(n) === after) buf.resize(to); } catch (e) {}
    return true;
  });
  if (typeof Int8Array === "function") {
    missing("TypedArray", Object.getPrototypeOf(Int8Array));
  }
  missing("testWithIntlConstructors", function (cb) {
    try {
      eachCtor(cb, Object.getOwnPropertyNames(Intl).filter(function (n) {
        return /^[A-Z]/.test(n);
      }), Intl);
    } catch (e) {}
  });

  // test262: $262. jsc names the same object `$`.
  function make262() {
    var r = {
      global: g,
      gc: g.gc,
      clearKeptObjects: noop,
      agent: sink({ monotonicNow: zero }),
      detachArrayBuffer: function (ab) {
        try { if (ab && typeof ab.transfer === "function") ab.transfer(); }
        catch (e) {}
      },
      evalScript: function (src) { return evalIn(r.global, src); },
      createRealm: function () {
        var o = make262();
        o.global = newGlobal();
        return o;
      }
    };
    return r;
  }
  var r262 = make262();
  missing("$262", r262);
  missing("$", g.$262);
  missing("$DETACHBUFFER", function (ab) { r262.detachArrayBuffer(ab); });

  // mjsunit.
  var asserts = ["assertEquals", "assertNotEquals", "assertSame",
    "assertNotSame", "assertTrue", "assertFalse", "assertNull",
    "assertNotNull", "assertEqualsDelta", "assertArrayEquals",
    "assertPropertiesEqual", "assertInstanceof", "assertUnreachable",
    "assertContains", "assertMatches", "assertOptimized",
    "assertUnoptimized", "assertMaglevved", "assertNotMaglevved", "fail",
    "shouldBe", "assertToStringOrNativeFunction", "reportCompare",
    "testRegExp"];
  for (var i = 0; i < asserts.length; i++) g[asserts[i]] = noop;
  g.assertThrows = g.assertThrowsEquals = g.assertDoesNotThrow =
    g.assertEarlyError = g.shouldThrow = g.shouldNotThrow =
    g.assertThrowsValue = g.shouldBeTrue =
    function (code) { run(code); };
  g.assertThrowsAsync = function (p) { settle(p); };
  g.assertPromiseResult = settle;
  missing("MjsUnitAssertionError", errorType());
  missing("__v8native", function () { return noop; });

  // d8 globals.
  missing("Realm", sink());
  missing("d8", sink());
  missing("Sandbox", sink());
  missing("Worker", function () { return sink(); });
  missing("performance", { now: zero });
  missing("setTimeout", function (fn) {
    try { Promise.resolve().then(function () { run(fn); }); }
    catch (e) { run(fn); }
    return 0;
  });

  // JSTests, with its ChakraCore, Mozilla, and LayoutTests files.
  missing("$vm", sink());
  missingNoops(["noInline", "noDFG", "noFTL", "noOSRExitFuzzing", "OSRExit",
    "fullGC", "edenGC", "ensureArrayStorage", "transferArrayBuffer",
    "drainMicrotasks", "description", "startTest", "inSection", "enterFunc",
    "printBugNumber", "writeHeaderToLog", "printStatus", "TestCase",
    "AddTestCase", "writeTestCaseResult", "test", "debug", "stopTest",
    "exitFunc"]);
  missing("tc", 0);
  missing("numberOfDFGCompiles", zero);
  missing("isFinalTier", function () { return false; });
  missing("testLoopCount", 10000);
  missing("fiatInt52", function (x) { return x; });
  missing("createGlobalObject", newGlobal);
  missing("runString", function (code) {
    var o = newGlobal();
    evalIn(o, code);
    return o;
  });
  missing("testRunner", sink());
  missing("WScript", sink({
    Arguments: [],
    SetTimeout: function (fn) { run(function () { g.setTimeout(fn, 0); }); },
    LoadScript: function (code) { evalIn(g, code); return g; }
  }));
})(Function("return this")());
