// Destructuring and spread
const [first, , third = 3, ...rest] = [1, 2, undefined, 4, 5];
const { name, age: years = 18, ...others } = { name: "x", extra: true };
function f({ a, b = 2 }, [c, d]) {
  return a + b + c + d;
}
let p, q;
[p, q] = [q, p];
({ p, q } = { p: 1, q: 2 });
const merged = { ...others, name, list: [...rest, first] };
for (const [key, value] of Object.entries(merged)) {
  console.log(key, value);
}
