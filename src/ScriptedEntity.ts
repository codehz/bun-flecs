import { Entity } from "./Entity";
import symbols from "./symbols";

export class ScriptedEntity extends Entity {
  update(code: string, template: bigint = 0n) {
    const buffer = new TextEncoder().encode(code + "\0");
    symbols.ecs_script_update(this.world, this.native, template, buffer);
  }
}
