import { cc, type ConvertFns, type FFIFunction } from "bun:ffi";

export const INCLUDE_DIR = new URL("../c-src", import.meta.url).pathname;

export function loadExtension<const Fns extends Record<string, FFIFunction>>(
  filename: string,
  symbols: Fns
): ConvertFns<Fns> {
  return cc({
    source: filename,
    include: [INCLUDE_DIR],
    symbols,
  }).symbols;
}
