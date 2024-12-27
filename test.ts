import { Member, Struct, World } from ".";


@Struct()
class Position{
  @Member("f64")
  x: number = 0;
  @Member("f64")
  y: number = 0;
}

@Struct()
class Velocity{
  @Member("f32")
  x: number = 0;
  @Member("f32")
  y: number = 0;
}

@Struct()
class Mass {
  @Member("f32")
  value: number = 0;
}

using world = new World();

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


console.log(entity.toString(), entity.type().map(x => x.toString()))


const a = world.lookup("a")!;

console.log(a?.toString())

for (const child of a.children) {
  console.log(child.toString())
}

const q = world.query("Position, Position(up)");

console.log(Bun.inspect(q.iter(), {depth: 100}))

using script = world.parse(`
d {
  Mass: { value: $mass }
  string: { $tag }
}`);
script.eval({mass: 5, tag: "tag"})

const d = world.lookup("d")!;

console.log(d?.toString())