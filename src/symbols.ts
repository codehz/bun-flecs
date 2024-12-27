import { dlopen, type Pointer } from "bun:ffi";

const lib = new URL("../flecs", import.meta.url).pathname;

const { symbols } = dlopen(lib, {
  ecs_init: { returns: "ptr" },
  ecs_fini: { args: ["ptr"] },
  ecs_quit: { args: ["ptr"] },
  ecs_progress: { args: ["ptr", "float"], returns: "bool" },

  ecs_new: { args: ["ptr"], returns: "u64" },
  ecs_delete: { args: ["ptr", "u64"] },
  ecs_add_id: { args: ["ptr", "u64", "u64"] },
  ecs_remove_id: { args: ["ptr", "u64", "u64"] },
  ecs_clear: { args: ["ptr", "u64"] },

  ecs_is_valid: { args: ["ptr", "u64"], returns: "bool" },
  ecs_is_alive: { args: ["ptr", "u64"], returns: "bool" },
  ecs_exists: { args: ["ptr", "u64"], returns: "bool" },

  ecs_enable: { args: ["ptr", "u64", "bool"] },
  ecs_enable_id: { args: ["ptr", "u64", "u64", "bool"] },
  ecs_is_enabled_id: { args: ["ptr", "u64", "u64"], returns: "bool" },
  ecs_get_id: { args: ["ptr", "u64", "u64"], returns: "ptr" },
  ecs_get_mut_id: { args: ["ptr", "u64", "u64"], returns: "ptr" },
  ecs_ensure_id: { args: ["ptr", "u64", "u64"], returns: "ptr" },
  ecs_modified_id: { args: ["ptr", "u64", "u64"], returns: "ptr" },
  ecs_ensure_modified_id: { args: ["ptr", "u64", "u64"], returns: "ptr" },

  ecs_get_type_js: { args: ["napi_env", "ptr", "u64"], returns: "napi_value" },
  ecs_entity_str_js: {
    args: ["napi_env", "ptr", "u64"],
    returns: "napi_value",
  },
  ecs_has_id: { args: ["ptr", "u64", "u64"], returns: "bool" },
  ecs_owns_id: { args: ["ptr", "u64", "u64"], returns: "bool" },
  ecs_get_parent: { args: ["ptr", "u64"], returns: "u64" },
  ecs_count_id: { args: ["ptr", "u64"], returns: "int" },
  ecs_children_js: { args: ["napi_env", "ptr", "u64"], returns: "napi_value" },

  ecs_get_name_js: { args: ["napi_env", "ptr", "u64"], returns: "napi_value" },
  ecs_get_symbol_js: {
    args: ["napi_env", "ptr", "u64"],
    returns: "napi_value",
  },
  ecs_set_name: { args: ["ptr", "u64", "cstring"], returns: "u64" },
  ecs_set_symbol: { args: ["ptr", "u64", "cstring"], returns: "u64" },
  ecs_set_alias: { args: ["ptr", "u64", "cstring"], returns: "u64" },
  ecs_lookup: { args: ["ptr", "cstring"], returns: "u64" },
  ecs_lookup_child: { args: ["ptr", "u64", "cstring"], returns: "u64" },
  ecs_lookup_symbol: { args: ["ptr", "cstring"], returns: "u64" },

  ecs_script_init_code: { args: ["ptr", "cstring"], returns: "u64" },
  ecs_script_update: { args: ["ptr", "u64", "u64", "cstring"], returns: "int" },
  ecs_script_clear: { args: ["ptr", "u64", "u64"] },

  ecs_script_parse: {
    args: ["ptr", "cstring", "cstring", "ptr"],
    returns: "ptr",
  },
  ecs_script_eval: { args: ["ptr", "ptr"], returns: "int" },
  ecs_script_free: { args: ["ptr"] },

  ecs_query_expr_js: {
    args: ["napi_env", "ptr", "cstring"],
    returns: "napi_value",
  },
  ecs_script_parse_js: {
    args: ["napi_env", "ptr", "cstring", "cstring"],
    returns: "napi_value",
  },

  ecs_populate_primitive_types_js: {
    args: ["napi_env"],
    returns: "napi_value",
  },
  get_bindings: { args: ["napi_env"], returns: "napi_value" },
} as const);

const bindings = symbols.get_bindings(null) as {
  bindings_create_type(
    world: Pointer,
    entity: bigint,
    definition: unknown
  ): bigint;
};

export default { ...symbols, ...bindings };
