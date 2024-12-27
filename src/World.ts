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

  parse(code: string, name = "<input>") {
    return symbols.ecs_script_parse_js(
      null,
      this.native,
      utf8(name),
      utf8(code)
    ) as {
      eval(vars?: Record<string, boolean | number | string>): void;
      [Symbol.dispose](): void;
    };
  }

  query(expr: string) {
    const raw = symbols.ecs_query_expr_js(null, this.native, utf8(expr)) as {
      exec(opt: any): string;
      [Symbol.dispose](): void;
    };
    return {
      exec<T extends unknown>(options?: {
        variables?: Record<string, string | bigint | Entity>;
        table?: boolean;
        builtin?: boolean;
        inherited?: boolean;
        matches?: boolean;
      }): T[] {
        return JSON.parse(raw.exec(options)).results;
      },
      [Symbol.dispose]() {
        return raw[Symbol.dispose]();
      },
    };
  }

  [Symbol.dispose]() {
    symbols.ecs_fini(this.native);
  }
}
