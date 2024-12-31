import { World } from ".";

using world = new World();

world.parse(`
struct Position {
  x = f64
  y = f64
}
struct Velocity {
  x = f32
  y = f32
}
struct Mass {
  value = f32
}
`).eval()

const entity = world.new_scripted(`
a {
  Position: { x: 0.1, y: 0.2 }
  b {
    Position: { x: 0.3, y: 0.4 }
    Velocity: { x: 0.1, y: 0.2 }
    c {
      Position: { x: 0.5, y: 0.6 }
      Mass: { value: 5 }
    }
  }
  c {}
}
`);


console.log(entity.toString(), entity.type().map(x => x.toJSON()))


const a = world.lookup("a")!;

console.log(a?.toString())

for (const child of a.children) {
  console.log(child.toJSON())
}

const q = world.query("$comp, $comp(up)");

console.log(Bun.inspect(q.exec({variables: {comp: "Position"}, matches: true, builtin: true}), {depth: 100, colors: true}))

using script = world.parse(`
d {
  Mass: { value: $mass }
  string: { $tag }
}`);
script.eval({mass: 5, tag: "tag"})

const d = world.lookup("d")!;

console.log(d?.toJSON())