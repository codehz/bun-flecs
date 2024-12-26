export function utf8(str: string) {
  return new TextEncoder().encode(str + "\0");
}
