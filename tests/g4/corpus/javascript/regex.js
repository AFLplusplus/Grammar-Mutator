// Regular expression literals
const re1 = /ab+c/gi;
const re2 = /^(\d{3})-(\d{4})$/;
const re3 = /[a-z/]+\/path/u;
const re4 = /(?<year>\d{4})-(?<month>\d{2})/;
const text = "2024-05 and 2025-06";
console.log(text.replace(re1, "x"), re2.test("555-1234"));
const half = 10 / 2 / 5;
const m = text.match(re4);
if (m && /\d/.test(m.groups.year)) {
  console.log(half, m.groups.month);
}