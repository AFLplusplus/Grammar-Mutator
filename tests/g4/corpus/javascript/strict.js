"use strict";
// Strict mode file, with text outside ASCII: café, 日本語
function strictFn() {
  "use strict";
  const label = "naïve ☃";
  return label.length;
}
var o = Object.freeze({ k: 1 });
console.log(strictFn(), o.k, typeof undefined, void 0);
