#include "./flecs.h"
#include "./js_native_api.h"

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

static napi_value ecsChildrenNext(napi_env env, napi_callback_info info) {
  napi_value result;
  ecs_iter_t *iter;
  napi_get_cb_info(env, info, &(size_t){0}, NULL, NULL, (void **)&iter);
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

static void freeIter(napi_env env, void *data, void *hint) {
  ecs_iter_fini(data);
  ecs_os_free(data);
}

napi_value ecs_children_js(napi_env env, const ecs_world_t *world,
                           ecs_entity_t parent) {
  napi_value result;
  ecs_iter_t *iter = ecs_os_malloc_t(ecs_iter_t);
  *iter = ecs_children(world, parent);
  napi_create_function(env, "ecs_children_next", 0, ecsChildrenNext, iter,
                       &result);
  napi_add_finalizer(env, result, iter, freeIter, NULL, NULL);
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

static napi_value ecsQueryTable(napi_env env, napi_callback_info info) {
  napi_value result;
  ecs_query_t *query;
  napi_get_cb_info(env, info, &(size_t){0}, NULL, NULL, (void **)&query);
  ecs_iter_t iter = ecs_query_iter(query->world, query);
  ecs_iter_to_json_desc_t desc = ECS_ITER_TO_JSON_INIT;
  desc.serialize_table = true;
  char *str = ecs_iter_to_json(&iter, &desc);
  napi_create_string_utf8(env, str, NAPI_AUTO_LENGTH, &result);
  ecs_os_free(str);
  return result;
}

static napi_value ecsQueryStr(napi_env env, napi_callback_info info) {
  napi_value result;
  ecs_query_t *query;
  napi_get_cb_info(env, info, &(size_t){0}, NULL, NULL, (void **)&query);
  char *str = ecs_query_str(query);
  napi_create_string_utf8(env, str, NAPI_AUTO_LENGTH, &result);
  ecs_os_free(str);
  return result;
}

static napi_value ecsQueryPlan(napi_env env, napi_callback_info info) {
  napi_value result;
  ecs_query_t *query;
  napi_get_cb_info(env, info, &(size_t){0}, NULL, NULL, (void **)&query);
  char *str = ecs_query_plan(query);
  napi_create_string_utf8(env, str, NAPI_AUTO_LENGTH, &result);
  ecs_os_free(str);
  return result;
}

static napi_value ecsQueryFindVar(napi_env env, napi_callback_info info) {
  napi_value result, name_;
  ecs_query_t *query;
  size_t argc = 1;
  napi_get_cb_info(env, info, &argc, &name_, NULL, (void **)&query);
  size_t namelen = 0;
  napi_status status =
      napi_get_value_string_utf8(env, name_, NULL, 0, &namelen);
  if (status != napi_ok || namelen <= 0)
    return napi_throw_error(env, NULL, "invalid name");
  char *name = ecs_os_malloc_n(char, namelen + 1);
  napi_get_value_string_utf8(env, name_, name, namelen + 1, &namelen);
  int32_t id = ecs_query_find_var(query, name);
  ecs_os_free(name);
  napi_create_int32(env, id, &result);
  return result;
}

static napi_value ecsQueryVarName(napi_env env, napi_callback_info info) {
  napi_value result, var_id_;
  ecs_query_t *query;
  size_t argc = 1;
  napi_get_cb_info(env, info, &argc, &var_id_, NULL, (void **)&query);
  int32_t var_id;
  napi_status status = napi_get_value_int32(env, var_id_, &var_id);
  if (status != napi_ok)
    return napi_throw_error(env, NULL, "invalid var id");
  char const *name = ecs_query_var_name(query, var_id);
  napi_create_string_utf8(env, name, NAPI_AUTO_LENGTH, &result);
  return result;
}

static napi_value ecsQueryVarIsEntity(napi_env env, napi_callback_info info) {
  napi_value result, var_id_;
  ecs_query_t *query;
  size_t argc = 1;
  napi_get_cb_info(env, info, &argc, &var_id_, NULL, (void **)&query);
  int32_t var_id;
  napi_status status = napi_get_value_int32(env, var_id_, &var_id);
  if (status != napi_ok)
    return napi_throw_error(env, NULL, "invalid var id");
  bool is_entity = ecs_query_var_is_entity(query, var_id);
  napi_get_boolean(env, is_entity, &result);
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
  napi_create_function(env, "ecs_query_table", 0, ecsQueryTable, query, &fn);
  napi_set_named_property(env, result, "table", fn);
  napi_create_function(env, "ecs_query_str", 0, ecsQueryStr, query, &fn);
  napi_set_named_property(env, result, "str", fn);
  napi_create_function(env, "ecs_query_plan", 0, ecsQueryPlan, query, &fn);
  napi_set_named_property(env, result, "plan", fn);
  napi_create_function(env, "ecs_query_find_var", 1, ecsQueryFindVar, query,
                       &fn);
  napi_set_named_property(env, result, "find_var", fn);
  napi_create_function(env, "ecs_query_var_name", 1, ecsQueryVarName, query,
                       &fn);
  napi_set_named_property(env, result, "var_name", fn);
  napi_create_function(env, "ecs_query_var_is_entity", 1, ecsQueryVarIsEntity,
                       query, &fn);
  napi_set_named_property(env, result, "var_is_entity", fn);
  napi_create_function(env, "ecs_query_args_parse", 1, ecsQueryArgsParse, query,
                       &fn);
  napi_set_named_property(env, result, "args_parse", fn);
  napi_add_finalizer(env, result, query, freeQuery, NULL, NULL);
  return result;
}