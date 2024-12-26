import symbols from "./symbols";

export const primitives = symbols.ecs_populate_primitive_types_js(null) as {
  bool: bigint;
  char: bigint;
  byte: bigint;
  u8: bigint;
  u16: bigint;
  u32: bigint;
  u64: bigint;
  uptr: bigint;
  i8: bigint;
  i16: bigint;
  i32: bigint;
  i64: bigint;
  iptr: bigint;
  f32: bigint;
  f64: bigint;
  string: bigint;
  entity: bigint;
  id: bigint;
};

export type ReflectionMember = {
  name: string;
  type: keyof typeof primitives | string;
  count?: number;
};

export type ReflectionDefinition =
  | { type: "struct"; members: ReflectionMember[] }
  | { type: "enum"; constants: { name: string; value?: number }[] };

export const definitions = new Map<string, ReflectionDefinition>();

export const members = new Map<Object, ReflectionMember[]>();