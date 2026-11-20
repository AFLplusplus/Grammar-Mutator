// Classes with inheritance, accessors, and static members
class Animal {
  #secret = 1;
  static count = 0;

  constructor(name) {
    this.name = name;
    Animal.count++;
  }

  get label() { return `animal:${this.name}`; }
  set label(value) { this.name = value; }

  speak() {
    return this.name + " makes a sound";
  }

  static create(name) {
    return new Animal(name);
  }
}

class Dog extends Animal {
  speak() {
    return super.speak() + " (woof)";
  }
}

const d = new Dog("Rex");
console.log(d.speak(), Animal.count);
