// Template literals
const name = "world";
const n = 3;
const greeting = `Hello, ${name}!`;
const nested = `outer ${`inner ${n + 1} ${[1, 2].map(x => `<${x}>`).join("")}`} end`;
const multi = `line one
line two ${n * 2}`;
const tagged = String.raw`a\nb${n}`;
function tag(strings, ...values) {
  return strings.raw.join("|") + values.join(",");
}
console.log(tag`x${1}y${2}z`, greeting, nested, multi, tagged);
