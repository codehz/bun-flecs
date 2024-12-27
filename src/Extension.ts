import { cc, type ConvertFns, type FFIFunction } from "bun:ffi";

export const INCLUDE_DIR = new URL("../c-src", import.meta.url).pathname;

export function loadExtension<const Fns extends Record<string, FFIFunction>>(
  filename: string,
  symbols: Fns,
  { include, library }: { include?: string[]; library?: string[] } = {}
): ConvertFns<Fns> {
  return cc({
    source: filename,
    include: include ? [INCLUDE_DIR, ...include] : [INCLUDE_DIR],
    library,
    symbols,
  }).symbols;
}
