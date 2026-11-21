// Generators and iterators
function* range(start, end, step = 1) {
  for (let i = start; i < end; i += step) {
    yield i;
  }
}

function* chain(...iterables) {
  for (const it of iterables) {
    yield* it;
  }
}

const iterable = {
  *[Symbol.iterator]() {
    yield 1;
    yield 2;
  },
};

const gen = chain(range(0, 3), iterable);
console.log([...gen], gen.next());
