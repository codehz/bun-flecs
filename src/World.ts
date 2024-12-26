import { Entity } from "./Entity";
import { definitions, primitives } from "./reflection_data";
import { ScriptedEntity } from "./ScriptedEntity";
import symbols from "./symbols";
import { utf8 } from "./utils";

export class World {
  readonly native = symbols.ecs_init()!;
  constructor() {
    if (!this.native) throw new Error("failed to init ecs world");
    if (definitions.size) {
      for (const [name, definition] of definitions) {
        const entity = symbols.ecs_set_name(this.native, 0, utf8(name));
        symbols.bindings_create_type(
          this.native,
          entity,
          definition.type === "struct"
            ? {
                type: "struct",
                members: definition.members.map((member) => ({
                  ...member,
                  type:
                    member.type in primitives
                      ? // @ts-ignore
                        primitives[member.type]
                      : symbols.ecs_lookup(this.native, utf8(member.type)),
                })),
              }
            : definition
        );
      }
    }
  }

  tick(frame: number) {
    return symbols.ecs_progress(this.native, frame);
  }

  new() {
    const entity = symbols.ecs_new(this.native);
    return new Entity(this.native, entity);
  }

  new_named(name: string) {
    const entity = symbols.ecs_set_name(this.native, 0, utf8(name));
    return new Entity(this.native, entity);
  }

  new_scripted(code: string) {
    const buffer = utf8(code);
    const entity = symbols.ecs_script_init_code(this.native, buffer);
    return new ScriptedEntity(this.native, entity);
  }

  count(id: bigint) {
    return symbols.ecs_count_id(this.native, id);
  }

  lookup(path: string) {
    const buffer = utf8(path);
    const id = symbols.ecs_lookup(this.native, buffer);
    if (id) return new Entity(this.native, id);
    else return null;
  }

  lookupSymbol(symbol: string) {
    const buffer = utf8(symbol);
    const id = symbols.ecs_lookup_symbol(this.native, buffer);
    if (id) return new Entity(this.native, id);
    else return null;
  }

  query(expr: string) {
    const raw = symbols.ecs_query_expr_js(null, this.native, utf8(expr)) as {
      iter(): string;
      table(): string;
    };
    return {
      iter<T>() {
        const parsed = JSON.parse(raw.iter()) as {
          results: { parent: string; name: string; fields: T }[];
        };
        return parsed.results;
      },
      table() {
        const parsed = JSON.parse(raw.table()) as { results: any[] };
        return parsed.results;
      },
    };
  }

  [Symbol.dispose]() {
    symbols.ecs_fini(this.native);
  }
}
