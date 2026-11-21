// Arrow functions
const add = (a, b) => a + b;
const square = x => x * x;
const makeAdder = n => m => n + m;
const noArgs = () => ({ value: 42 });
const sum = (...xs) => xs.reduce((acc, x) => acc + x, 0);
const withBody = (a, b = 2) => {
  const c = a * b;
  return c + 1;
};
console.log([1, 2, 3].map(x => x * 2).filter(x => x > 2));
