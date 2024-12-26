export * from "./src/World";
export * from "./src/Entity";
export * from "./src/ScriptedEntity";
export * from "./src/Reflection";

export const INCLUDE_DIR = new URL("c-src", import.meta.url).pathname;
