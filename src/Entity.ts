import type { Pointer } from "bun:ffi";
import symbols from "./symbols";

export class Entity {
  constructor(readonly world: Pointer, readonly native: bigint) {}

  get name() {
    return (
      (symbols.ecs_get_name_js(null, this.world, this.native) as string) ?? ""
    );
  }
  set name(value: string) {
    const buffer = new TextEncoder().encode(value + "\0");
    symbols.ecs_set_name(this.world, this.native, buffer);
  }

  get symbol() {
    return (
      (symbols.ecs_get_symbol_js(null, this.world, this.native) as string) ?? ""
    );
  }
  set symbol(value: string) {
    const buffer = new TextEncoder().encode(value + "\0");
    symbols.ecs_set_symbol(this.world, this.native, buffer);
  }

  set alias(value: string) {
    const buffer = new TextEncoder().encode(value + "\0");
    symbols.ecs_set_alias(this.world, this.native, buffer);
  }

  lookup(path: string) {
    const buffer = new TextEncoder().encode(path + "\0");
    const id = symbols.ecs_lookup_child(this.world, this.native, buffer);
    if (id) return new Entity(this.world, id);
    else return null;
  }

  add(id: bigint | Entity) {
    symbols.ecs_add_id(
      this.world,
      this.native,
      typeof id === "bigint" ? id : id.native
    );
  }

  remove(id: bigint | Entity) {
    symbols.ecs_remove_id(
      this.world,
      this.native,
      typeof id === "bigint" ? id : id.native
    );
  }

  clear() {
    symbols.ecs_clear(this.world, this.native);
  }

  enable(id: bigint | Entity, enabled: boolean): void;
  enable(enabled: boolean): void;
  enable(a: any, b?: any) {
    if (b == null) {
      symbols.ecs_enable(this.world, this.native, a);
    } else {
      symbols.ecs_enable_id(
        this.world,
        this.native,
        typeof a === "bigint" ? a : a.native,
        b
      );
    }
  }

  isEnabled(id: bigint | Entity) {
    return symbols.ecs_is_enabled_id(
      this.world,
      this.native,
      typeof id === "bigint" ? id : id.native
    );
  }

  isValid() {
    return symbols.ecs_is_valid(this.world, this.native);
  }

  isAlive() {
    return symbols.ecs_is_alive(this.world, this.native);
  }

  isExists() {
    return symbols.ecs_exists(this.world, this.native);
  }

  get(id: bigint | Entity, mode: GetIdMode = 0) {
    id = typeof id === "bigint" ? id : id.native;
    switch (mode) {
      case GetIdMode.DEFAULT:
        return symbols.ecs_get_id(this.world, this.native, id);
      case GetIdMode.MUTABLE:
        return symbols.ecs_get_mut_id(this.world, this.native, id);
      case GetIdMode.ENSURE:
        return symbols.ecs_ensure_id(this.world, this.native, id);
      case GetIdMode.MODIFIED:
        return symbols.ecs_modified_id(this.world, this.native, id);
      case GetIdMode.ENSURE_MODIFIED:
        return symbols.ecs_ensure_modified_id(this.world, this.native, id);
    }
    throw new Error("invalid mode: " + mode);
  }

  has(id: bigint | Entity) {
    return symbols.ecs_has_id(
      this.world,
      this.native,
      typeof id === "bigint" ? id : id.native
    );
  }

  owns(id: bigint | Entity) {
    return symbols.ecs_owns_id(
      this.world,
      this.native,
      typeof id === "bigint" ? id : id.native
    );
  }

  type() {
    return (
      symbols.ecs_get_type_js(null, this.world, this.native) as bigint[]
    ).map((id) => new Entity(this.world, id));
  }

  toString() {
    return symbols.ecs_entity_str_js(null, this.world, this.native) as string;
  }

  toJSON() {
    return JSON.parse(
      symbols.ecs_entity_to_json_js(null, this.world, this.native) as string
    );
  }

  get parent(): Entity | null {
    const id = symbols.ecs_get_parent(this.world, this.native);
    if (id) {
      return new Entity(this.world, id);
    }
    return null;
  }

  get children(): IteratorObject<Entity> {
    const iterator = symbols.ecs_children_js(null, this.world, this.native) as {
      next(): bigint[] | undefined;
      done(): void;
    };
    return new EntityIter(this.world, iterator);
  }

  [Symbol.dispose]() {
    symbols.ecs_delete(this.world, this.native);
  }
}

class EntityIter extends Iterator<Entity> {
  #world: Pointer;
  #raw: {
    next(): bigint[] | undefined;
    done(): void;
  };
  #buffer: Entity[] = [];
  constructor(
    world: Pointer,
    raw: {
      next(): bigint[] | undefined;
      done(): void;
    }
  ) {
    super();
    this.#world = world;
    this.#raw = raw;
  }
  next(): IteratorResult<Entity, any> {
    if (this.#buffer.length) {
      return { done: false, value: this.#buffer.shift()! };
    }
    const array = this.#raw.next();
    if (!array) return { done: true, value: undefined };
    this.#buffer = array.map((item) => new Entity(this.#world, item));
    return this.next();
  }

  throw(): IteratorResult<Entity, any> {
    this.#raw.done();
    return { done: true, value: undefined };
  }

  return(): IteratorResult<Entity, any> {
    this.#raw.done();
    return { done: true, value: undefined };
  }
}

export enum GetIdMode {
  DEFAULT = 0,
  MUTABLE = 1,
  ENSURE = 2,
  MODIFIED = 4,
  ENSURE_MODIFIED = 6,
}
