// Modules
import defaultValue from "./a.js";
import * as everything from "./b.js";
import { one, two as deux } from "./c.js";
import def, { three } from "./d.js";
import "./side-effect.js";

export const answer = 42;
export function twice(x) { return x * 2; }
export class Box {}
export { one, deux as zwei };
export * from "./e.js";
export * as ns from "./f.js";
export default function () {
  return defaultValue + def + three;
}
