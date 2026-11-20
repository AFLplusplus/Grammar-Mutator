// Labels, loops, and switch
outer: for (let i = 0; i < 3; i++) {
  inner: for (let j = 0; j < 3; j++) {
    if (j === 1) continue outer;
    if (i === 2) break outer;
    console.log(i, j);
  }
}
let k = 0;
while (k < 5) k++;
do {
  k--;
} while (k > 0);
for (const key in { a: 1, b: 2 }) console.log(key);
switch (k) {
  case 0:
    console.log("zero");
    break;
  default:
    console.log("other");
}
