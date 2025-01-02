#include "./flecs.h"
#include "./js_native_api.h"
#include "./js_native_api_types.h"

#define TRY_(expr, label)                                                      \
  if ((status = expr) != napi_ok)                                              \
  goto label
#define TRY0(expr)                                                             \
  if ((status = expr) != napi_ok)                                              \
  return status

typedef struct allocpool {
  struct allocpool *prev;
  size_t used, length;
  char buffer[];
} *allocpool_t, **allocpool_ref_t;

static size_t nextsize(size_t size, size_t initial) {
  size_t power = initial;
  while (power < size)
    power *= 2;
  return power;
}

static void *allocpool_alloc(allocpool_ref_t ppool, size_t size) {
  if (!*ppool || size > (*ppool)->length - (*ppool)->used) {
    size_t alloc = nextsize(size + sizeof(allocpool_t), 256);
    allocpool_t pool = ecs_os_calloc(alloc);
    pool->prev = *ppool;
    *ppool = pool;
  }
  void *result = (*ppool)->buffer + (*ppool)->used;
  (*ppool)->used += size;
  return result;
}

static void allocpool_fini(allocpool_t pool) {
  while (pool) {
    allocpool_t temp = pool;
    pool = pool->prev;
    ecs_os_free(temp);
  }
}

static napi_status jsSymbolDispose(napi_env env, napi_value *target) {
  napi_status status;
  TRY0(napi_get_global(env, target));
  TRY0(napi_get_named_property(env, *target, "Symbol", target));
  TRY0(napi_get_named_property(env, *target, "dispose", target));
  return napi_ok;
}

ecs_entity_t ecs_script_init_code(ecs_world_t *world, char const *code) {
  return ecs_script(world, {.code = code});
}

napi_value ecs_get_type_js(napi_env env, ecs_world_t const *world,
                           ecs_entity_t entity) {
  ecs_type_t const *type = ecs_get_type(world, entity);
  napi_value result;
  napi_create_array_with_length(env, type->count, &result);
  for (int i = 0; i < type->count; i++) {
    napi_value value;
    napi_create_bigint_uint64(env, type->array[i], &value);
    napi_set_element(env, result, i, value);
  }
  return result;
}

napi_value ecs_entity_str_js(napi_env env, ecs_world_t const *world,
                             ecs_entity_t entity) {
  char *str = ecs_entity_str(world, entity);
  napi_value result;
  napi_create_string_utf8(env, str, NAPI_AUTO_LENGTH, &result);
  ecs_os_free(str);
  return result;
}

napi_value ecs_entity_to_json_js(napi_env env, ecs_world_t const *world,
                                 ecs_entity_t entity) {
  ecs_entity_to_json_desc_t desc = ECS_ENTITY_TO_JSON_INIT;
  char *str = ecs_entity_to_json(world, entity, &desc);
  napi_value result;
  napi_create_string_utf8(env, str, NAPI_AUTO_LENGTH, &result);
  ecs_os_free(str);
  return result;
}

napi_value ecs_get_name_js(napi_env env, ecs_world_t const *world,
                           ecs_entity_t entity) {
  char const *str = ecs_get_name(world, entity);
  napi_value result;
  if (str) {
    napi_create_string_utf8(env, str, NAPI_AUTO_LENGTH, &result);
    return result;
  } else {
    napi_get_null(env, &result);
    return result;
  }
}

napi_value ecs_get_symbol_js(napi_env env, ecs_world_t const *world,
                             ecs_entity_t entity) {
  char const *str = ecs_get_symbol(world, entity);
  napi_value result;
  if (str) {
    napi_create_string_utf8(env, str, NAPI_AUTO_LENGTH, &result);
    return result;
  } else {
    napi_get_null(env, &result);
    return result;
  }
}

static napi_value jsChildrenNext(napi_env env, napi_callback_info info) {
  napi_value result;
  ecs_iter_t *iter;
  napi_get_cb_info(env, info, &(size_t){0}, NULL, NULL, (void **)&iter);
  if (!iter) {
    napi_throw_error(env, NULL, "Invalid iterator");
    return NULL;
  }
  if (ecs_children_next(iter)) {
    napi_create_array_with_length(env, iter->count, &result);
    for (int i = 0; i < iter->count; i++) {
      napi_value value;
      napi_create_bigint_uint64(env, iter->entities[i], &value);
      napi_set_element(env, result, i, value);
    }
  } else {
    napi_get_undefined(env, &result);
  }
  return result;
}

static napi_value jsChildrenDone(napi_env env, napi_callback_info info) {
  napi_value result;
  ecs_iter_t *iter = NULL;
  napi_get_cb_info(env, info, &(size_t){0}, NULL, NULL, (void **)&iter);
  if (!iter) {
    napi_throw_error(env, NULL, "Invalid iterator");
    return NULL;
  }
  napi_get_undefined(env, &result);
  ecs_iter_fini(iter);
  ecs_os_free(iter);
  return result;
}

napi_value ecs_children_js(napi_env env, const ecs_world_t *world,
                           ecs_entity_t parent) {
  napi_value result, fn;
  ecs_iter_t *iter = ecs_os_malloc_t(ecs_iter_t);
  *iter = ecs_children(world, parent);
  napi_create_object(env, &result);
  napi_create_function(env, "next", 0, jsChildrenNext, iter, &fn);
  napi_set_named_property(env, result, "next", fn);
  napi_create_function(env, "done", 0, jsChildrenDone, iter, &fn);
  napi_set_named_property(env, result, "done", fn);
  return result;
}

static napi_valuetype jsType(napi_env env, napi_value value) {
  napi_valuetype vt;
  napi_typeof(env, value, &vt);
  return vt;
}

static bool jsGetBoolFlag(napi_env env, napi_value value, char const *prop) {
  napi_value temp;
  bool result;
  napi_get_named_property(env, value, prop, &temp);
  napi_coerce_to_bool(env, temp, &temp);
  napi_get_value_bool(env, temp, &result);
  return result;
}

static ecs_entity_t jsGetNativeHandle(napi_env env, napi_value value) {
  napi_value temp;
  ecs_entity_t result;
  napi_get_named_property(env, value, "native", &temp);
  napi_get_value_bigint_uint64(env, temp, &result, NULL);
  return result;
}

static napi_value ecsQueryExec(napi_env env, napi_callback_info info) {
  napi_value result, arg;
  ecs_query_t *query;
  size_t argc = 1;
  napi_get_cb_info(env, info, &argc, &arg, NULL, (void **)&query);
  ecs_iter_t iter = ecs_query_iter(query->world, query);
  ecs_iter_to_json_desc_t desc = ECS_ITER_TO_JSON_INIT;
  if (argc == 1 && jsType(env, arg) == napi_object) {
    napi_value vars;
    napi_get_named_property(env, arg, "variables", &vars);
    if (jsType(env, vars) == napi_object) {
      napi_value props;
      uint32_t props_len;
      napi_get_property_names(env, vars, &props);
      napi_get_array_length(env, props, &props_len);
      char *strbuf = NULL;
      for (uint32_t i = 0; i < props_len; i++) {
        napi_value key, value;
        size_t keylen, vallen;
        ecs_entity_t target;
        napi_get_element(env, props, i, &key);
        napi_get_property(env, vars, key, &value);
        napi_get_value_string_utf8(env, key, NULL, 0, &keylen);
        strbuf = ecs_os_realloc(strbuf, nextsize(keylen + 1, 256));
        napi_get_value_string_utf8(env, key, strbuf, keylen + 1, &keylen);
        int32_t varidx = ecs_query_find_var(query, strbuf);
        if (varidx == 0)
          continue;
        switch (jsType(env, value)) {
        case napi_string:
          napi_get_value_string_utf8(env, value, NULL, 0, &vallen);
          strbuf = ecs_os_realloc(strbuf, nextsize(vallen + 1, 256));
          napi_get_value_string_utf8(env, value, strbuf, vallen + 1, &vallen);
          target = ecs_lookup(iter.world, strbuf);
          ecs_iter_set_var(&iter, varidx, target);
          break;
        case napi_bigint:
          napi_get_value_bigint_uint64(env, value, &target, NULL);
          ecs_iter_set_var(&iter, varidx, target);
          break;
        case napi_object:
          target = jsGetNativeHandle(env, value);
          ecs_iter_set_var(&iter, varidx, target);
          break;
        default:
          break;
        }
      }
      ecs_os_free(strbuf);
    }
    desc.serialize_table = jsGetBoolFlag(env, arg, "table");
    desc.serialize_builtin = jsGetBoolFlag(env, arg, "builtin");
    desc.serialize_inherited = jsGetBoolFlag(env, arg, "inherited");
    desc.serialize_matches = jsGetBoolFlag(env, arg, "matches");
  }
  char *str = ecs_iter_to_json(&iter, &desc);
  napi_create_string_utf8(env, str, NAPI_AUTO_LENGTH, &result);
  ecs_os_free(str);
  return result;
}

static napi_value ecsQueryDispose(napi_env env, napi_callback_info info) {
  napi_value result;
  ecs_query_t *query;
  napi_get_cb_info(env, info, &(size_t){0}, NULL, NULL, (void **)&query);
  ecs_query_fini(query);
  napi_get_undefined(env, &result);
  return result;
}

napi_value ecs_query_expr_js(napi_env env, ecs_world_t *world,
                             char const *expr) {
  napi_value result, fn, dispose;
  ecs_query_t *query = ecs_query(world, {.expr = expr});
  if (!query) {
    napi_throw_error(env, NULL, "Query failed");
    return NULL;
  }
  jsSymbolDispose(env, &dispose);
  napi_create_object(env, &result);
  napi_create_function(env, "ecs_query_exec", 0, ecsQueryExec, query, &fn);
  napi_set_named_property(env, result, "exec", fn);
  napi_create_function(env, "ecs_query_dispose", 0, ecsQueryDispose, query,
                       &fn);
  napi_set_property(env, result, dispose, fn);
  return result;
}

static napi_status jsValueToEcsVar(napi_env env, ecs_script_vars_t *vars,
                                   char *name, napi_value value) {
  napi_status status = napi_ok;
  napi_valuetype type;
  ecs_script_var_t *var;

  TRY0(napi_typeof(env, value, &type));
  switch (type) {
  case napi_boolean:
    var = ecs_script_vars_define(vars, name, ecs_bool_t);
    TRY0(napi_get_value_bool(env, value, var->value.ptr));
    break;
  case napi_number:
    var = ecs_script_vars_define(vars, name, ecs_f64_t);
    TRY0(napi_get_value_double(env, value, var->value.ptr));
    break;
  case napi_string: {
    var = ecs_script_vars_define(vars, name, ecs_string_t);
    size_t valuelen = 0;
    TRY0(napi_get_value_string_utf8(env, value, NULL, 0, &valuelen));
    char *str = *(char **)var->value.ptr = ecs_os_malloc_n(char, valuelen + 1);
    TRY0(napi_get_value_string_utf8(env, value, str, valuelen + 1, &valuelen));
  } break;
  default:
    break;
  }

  return status;
}

static napi_status jsObjectToEcsVars(napi_env env, ecs_script_vars_t *vars,
                                     napi_value object, allocpool_ref_t ppool) {
  napi_status status = napi_ok;
  napi_value properties;
  napi_get_property_names(env, object, &properties);
  uint32_t length = 0;
  napi_get_array_length(env, properties, &length);
  for (int i = 0; i < length; i++) {
    napi_handle_scope scope;
    char *keybuf = 0;
    napi_open_handle_scope(env, &scope);

#define TRY(expr) TRY_(expr, loop_error)

    napi_value key, value;
    TRY(napi_get_element(env, properties, i, &key));
    TRY(napi_get_property(env, object, key, &value));

    size_t keylen = 0;
    TRY(napi_get_value_string_utf8(env, key, NULL, 0, &keylen));

    keybuf = allocpool_alloc(ppool, keylen + 1);
    TRY(napi_get_value_string_utf8(env, key, keybuf, keylen + 1, &keylen));

    TRY(jsValueToEcsVar(env, vars, keybuf, value));

#undef TRY

  loop_error:
    napi_close_handle_scope(env, scope);
    if (status)
      return status;
  }
  return status;
}

static napi_value evalScript(napi_env env, napi_callback_info info) {
  allocpool_t pool = NULL;
  napi_value result;
  ecs_script_t *script;
  size_t argc = 1;
  napi_value vars;
  if (napi_get_cb_info(env, info, &argc, &vars, NULL, (void **)&script)) {
    napi_throw_error(env, NULL, "failed to get callback info");
    return NULL;
  }
  ecs_script_eval_desc_t desc = {};
  if (argc == 1) {
    desc.vars = ecs_script_vars_init(script->world);
    napi_status status = jsObjectToEcsVars(env, desc.vars, vars, &pool);
    if (status != napi_ok) {
      napi_throw_error(env, NULL, "failed to convert flecs vars");
      return NULL;
    }
  }
  int script_result = ecs_script_eval(script, &desc);
  if (desc.vars) {
    ecs_script_vars_fini(desc.vars);
  }
  if (pool) {
    allocpool_fini(pool);
  }
  if (script_result) {
    napi_throw_error(env, NULL, "eval error");
    return NULL;
  }
  napi_get_undefined(env, &result);
  return result;
}

static napi_value disposeScript(napi_env env, napi_callback_info info) {
  napi_value result;
  ecs_script_t *script;
  if (napi_get_cb_info(env, info, &(size_t){0}, NULL, NULL, (void **)&script)) {
    napi_throw_error(env, NULL, "failed to get callback info");
    return NULL;
  }
  ecs_script_free(script);
  napi_get_undefined(env, &result);
  return result;
}

napi_value ecs_script_parse_js(napi_env env, ecs_world_t *world, char *name,
                               char *code) {
  napi_value result, dispose, fn;
  ecs_script_t *script = ecs_script_parse(world, name, code, NULL);
  if (!script) {
    napi_throw_error(env, NULL, "failed to parse code");
    return NULL;
  }
  if (napi_create_object(env, &result) || jsSymbolDispose(env, &dispose) ||
      napi_create_function(env, "eval", 1, evalScript, script, &fn) ||
      napi_set_named_property(env, result, "eval", fn) ||
      napi_create_function(env, "dispose", 1, disposeScript, script, &fn) ||
      napi_set_property(env, result, dispose, fn) != napi_ok) {
    napi_throw_error(env, NULL, "failed to create object");
    return NULL;
  }
  return result;
}

napi_value ecs_world_to_json_js(napi_env env, ecs_world_t *world) {
  ecs_world_to_json_desc_t desc = {};
  char *json = ecs_world_to_json(world, &desc);
  napi_value result;
  napi_create_string_utf8(env, json, NAPI_AUTO_LENGTH, &result);
  ecs_os_free(json);
  return result;
}
