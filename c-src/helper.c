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

static size_t nextsize(size_t size) {
  size_t power = 4096;
  while (power < size)
    power *= 2;
  return power;
}

static void *allocpool_alloc(allocpool_ref_t ppool, size_t size) {
  if (!*ppool || size > (*ppool)->length - (*ppool)->used) {
    size_t alloc = nextsize(size + sizeof(allocpool_t));
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

napi_value ecs_populate_primitive_types_js(napi_env env) {
  napi_value result, temp;
  napi_create_object(env, &result);
#define SET(name)                                                              \
  napi_create_bigint_uint64(env, ecs_id(ecs_##name##_t), &temp);               \
  napi_set_named_property(env, result, #name, temp)
  SET(bool);
  SET(char);
  SET(byte);
  SET(u8);
  SET(u16);
  SET(u32);
  SET(u64);
  SET(uptr);
  SET(i8);
  SET(i16);
  SET(i32);
  SET(i64);
  SET(iptr);
  SET(f32);
  SET(f64);
  SET(string);
  SET(entity);
  SET(id);
#undef SET
  return result;
}

static napi_value create_type_impl(napi_env env, ecs_world_t *world,
                                   ecs_entity_t target, napi_value value) {
  napi_value type, result;
  napi_get_named_property(env, value, "type", &type);
  char buffer[32] = {0};
  size_t size = 0;
  napi_get_value_string_utf8(env, type, buffer, sizeof buffer, &size);
  if (ecs_os_strcmp(buffer, "struct") == 0) {
    ecs_struct_desc_t desc = {.entity = target};
    napi_value members;
    uint32_t length;
    ecs_strbuf_t names_buffer = ECS_STRBUF_INIT;
    napi_get_named_property(env, value, "members", &members);
    napi_get_array_length(env, members, &length);
    if (length > ECS_MEMBER_DESC_CACHE_SIZE) {
      return napi_throw_type_error(env, NULL, "Too many members in struct");
    }
    for (int i = 0; i < length; i++) {
      napi_value member, name, type, count;
      napi_get_element(env, members, i, &member);
      napi_get_named_property(env, member, "name", &name);
      napi_get_named_property(env, member, "type", &type);
      napi_get_named_property(env, member, "count", &count);

      size_t namelen = 0;
      napi_get_value_string_utf8(env, name, NULL, 0, &namelen);
      char *namebuf = ecs_os_malloc_n(char, namelen + 1);
      desc.members[i].name = namebuf;
      napi_get_value_string_utf8(env, name, namebuf, namelen + 1, &namelen);
      napi_get_value_bigint_uint64(env, type, &desc.members[i].type,
                                   &(bool){true});
      napi_get_value_int32(env, count, &desc.members[i].count);
    }
    ecs_entity_t entity = ecs_struct_init(world, &desc);
    for (int i = 0; i < length; i++) {
      if (desc.members[i].name) {
        ecs_os_free((void *)desc.members[i].name);
      }
    }
    napi_create_bigint_uint64(env, entity, &result);
    return result;
  } else if (ecs_os_strcmp(buffer, "enum") == 0) {
    ecs_enum_desc_t desc = {.entity = target};
    napi_value constants;
    uint32_t length;
    napi_get_named_property(env, value, "constants", &constants);
    napi_get_array_length(env, constants, &length);
    if (length > ECS_MEMBER_DESC_CACHE_SIZE) {
      return napi_throw_type_error(env, NULL, "Too many members in struct");
    }
    for (int i = 0; i < length; i++) {
      napi_value constant, name, value;
      int32_t int32value;
      napi_get_element(env, constants, i, &constant);
      napi_get_named_property(env, constant, "name", &name);
      napi_get_named_property(env, constant, "value", &value);

      size_t namelen = 0;
      napi_get_value_string_utf8(env, name, NULL, 0, &namelen);
      char *namebuf = ecs_os_malloc_n(char, namelen + 1);
      desc.constants[i].name = namebuf;
      napi_get_value_int32(env, name, &int32value);
      desc.constants[i].value = int32value;
    }
    ecs_entity_t entity = ecs_enum_init(world, &desc);
    for (int i = 0; i < length; i++) {
      if (desc.constants[i].name) {
        ecs_os_free((void *)desc.constants[i].name);
      }
    }
    napi_create_bigint_uint64(env, entity, &result);
    return result;
  }
  return napi_throw_error(env, NULL, "not implemented");
}

static napi_value createType(napi_env env, napi_callback_info info) {
  napi_value result = 0, world_, target_;
  size_t argc = 3;
  napi_value argv[3] = {};
  napi_get_cb_info(env, info, &argc, argv, NULL, NULL);
  if (argc != 3)
    return napi_throw_error(env, NULL, "invalid invocation");
  ecs_world_t *world = NULL;
  int64_t world_ptr = 0;
  ecs_entity_t target = 0;
  napi_value value = argv[2];
  napi_get_value_int64(env, argv[0], &world_ptr);
  world = (ecs_world_t *)world_ptr;
  napi_status status =
      napi_get_value_bigint_uint64(env, argv[1], &target, &(bool){true});
  return create_type_impl(env, world, target, value);
}

napi_value get_bindings(napi_env env) {
  napi_value fn, result;
  napi_create_object(env, &result);
  napi_create_function(env, "createType", 3, createType, NULL, &fn);
  napi_set_named_property(env, result, "bindings_create_type", fn);
  return result;
}

static napi_value jsChildrenNext(napi_env env, napi_callback_info info) {
  napi_value result;
  ecs_iter_t *iter;
  napi_get_cb_info(env, info, &(size_t){0}, NULL, NULL, (void **)&iter);
  if (!iter) return napi_throw_error(env, NULL, "Invalid iterator");
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
  if (!iter) return napi_throw_error(env, NULL, "Invalid iterator");
  ecs_iter_fini(iter);
  ecs_os_free(iter);
  return result;
}

static void freeIter(napi_env env, void *data, void *hint) {
  ecs_iter_fini(data);
  ecs_os_free(data);
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

static napi_value ecsQueryIter(napi_env env, napi_callback_info info) {
  napi_value result;
  ecs_query_t *query;
  napi_get_cb_info(env, info, &(size_t){0}, NULL, NULL, (void **)&query);
  ecs_iter_t iter = ecs_query_iter(query->world, query);
  ecs_iter_to_json_desc_t desc = ECS_ITER_TO_JSON_INIT;
  char *str = ecs_iter_to_json(&iter, &desc);
  napi_create_string_utf8(env, str, NAPI_AUTO_LENGTH, &result);
  ecs_os_free(str);
  return result;
}

static napi_value ecsQueryArgsParse(napi_env env, napi_callback_info info) {
  napi_value result, expr_;
  ecs_query_t *query;
  size_t argc = 1;
  napi_get_cb_info(env, info, &argc, &expr_, NULL, (void **)&query);

  size_t exprlen = 0;
  napi_status status =
      napi_get_value_string_utf8(env, expr_, NULL, 0, &exprlen);
  if (status != napi_ok || exprlen <= 0)
    return napi_throw_error(env, NULL, "invalid expr");
  char *expr = ecs_os_malloc_n(char, exprlen + 1);
  napi_get_value_string_utf8(env, expr_, expr, exprlen + 1, &exprlen);

  ecs_iter_t iter;
  ecs_query_args_parse(query, &iter, expr);
  ecs_iter_to_json_desc_t desc = ECS_ITER_TO_JSON_INIT;
  char *str = ecs_iter_to_json(&iter, &desc);
  napi_create_string_utf8(env, str, NAPI_AUTO_LENGTH, &result);
  ecs_os_free(str);
  ecs_os_free(expr);
  return result;
}

static void freeQuery(napi_env env, void *data, void *hint) {
  ecs_query_fini(data);
}

napi_value ecs_query_expr_js(napi_env env, ecs_world_t *world,
                             char const *expr) {
  napi_value result, fn;
  ecs_query_t *query = ecs_query(world, {.expr = expr});
  if (!query) {
    return napi_throw_error(env, NULL, "Query failed");
  }
  napi_create_object(env, &result);
  napi_create_function(env, "ecs_query_iter", 0, ecsQueryIter, query, &fn);
  napi_set_named_property(env, result, "iter", fn);
  napi_add_finalizer(env, result, query, freeQuery, NULL, NULL);
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
  if (napi_get_cb_info(env, info, &argc, &vars, NULL, (void **)&script))
    return napi_throw_error(env, NULL, "failed to get callback info");
  ecs_script_eval_desc_t desc = {};
  if (argc == 1) {
    desc.vars = ecs_script_vars_init(script->world);
    napi_status status = jsObjectToEcsVars(env, desc.vars, vars, &pool);
  }
  int script_result = ecs_script_eval(script, &desc);
  if (desc.vars) {
    ecs_script_vars_fini(desc.vars);
  }
  if (pool) {
    allocpool_fini(pool);
  }
  if (script_result) {
    return napi_throw_error(env, NULL, "eval error");
  }
  napi_get_undefined(env, &result);
  return result;
}

static napi_value disposeScript(napi_env env, napi_callback_info info) {
  napi_value result;
  ecs_script_t *script;
  if (napi_get_cb_info(env, info, &(size_t){0}, NULL, NULL, (void **)&script))
    return napi_throw_error(env, NULL, "failed to get callback info");
  ecs_script_free(script);
  napi_get_undefined(env, &result);
  return result;
}

napi_value ecs_script_parse_js(napi_env env, ecs_world_t *world, char *name,
                               char *code) {
  napi_value result, dispose, fn;
  ecs_script_t *script = ecs_script_parse(world, name, code, NULL);
  if (!script) {
    return napi_throw_error(env, NULL, "failed to parse code");
  }
  if (napi_create_object(env, &result) || jsSymbolDispose(env, &dispose) ||
      napi_create_function(env, "eval", 1, evalScript, script, &fn) ||
      napi_set_named_property(env, result, "eval", fn) ||
      napi_create_function(env, "dispose", 1, disposeScript, script, &fn) ||
      napi_set_property(env, result, dispose, fn) != napi_ok) {
    return napi_throw_error(env, NULL, "failed to create object");
  }
  return result;
}