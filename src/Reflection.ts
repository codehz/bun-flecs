import { definitions, members, type primitives } from "./reflection_data";

export function Struct(name?: string): ClassDecorator {
  return (clazz) => {
    definitions.set(name ?? clazz.name, {
      type: "struct",
      members: members.get(clazz.prototype) ?? [],
    });
    members.delete(clazz.prototype);
    return clazz;
  };
}

export function Member(
  type: keyof typeof primitives | (string & {}),
  count = 0
): PropertyDecorator {
  return (target, key) => {
    if (typeof key === "symbol") throw new TypeError("invalid field");
    let list = members.get(target);
    if (!list) members.set(target, (list = []));
    list.push({ name: key, type, count });
  };
}

export function Enum<const E extends string[]>(name: string, values: E) {
  const remap = {} as Record<E[number], number>;
  for (let i = 0; i < values.length; i++) {
    // @ts-ignore
    remap[values[i]] = i;
  }
  definitions.set(name, {
    type: "enum",
    constants: values.map((name) => ({ name })),
  });
  return remap;
}
