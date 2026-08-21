#include "pg_prelude.h"

#include <commands/extension.h>

#include "fdw.h"
#include "privileged_extensions.h"

/*
 * Returns true if the function indicated by `func_oid` is owned by
 * a non TLE extension, false otherwise.
 */
static bool is_func_owned_by_non_tle_extension(Oid func_oid) {
  if (!OidIsValid(func_oid)) return false;

  Oid extension_oid = getExtensionOfObject(ProcedureRelationId, func_oid);
  if (!OidIsValid(extension_oid)) return false;

  char *extension_name = get_extension_name(extension_oid);
  if (extension_name == NULL) return false;

  // Available extensions are not TLEs
  bool available = is_extension_available(extension_name);

  pfree(extension_name);

  return available;
}

/*
 * Given a handler `DefElem` returns its oid.
 */
static Oid lookup_fdw_handler_func(DefElem *handler) {
  if (handler == NULL || handler->arg == NULL) return InvalidOid;

  /* handlers have no arguments */
  Oid handler_oid = LookupFuncName((List *)handler->arg, 0, NULL, false);

  /* check that handler has correct return type */
  if (get_func_rettype(handler_oid) != FDW_HANDLEROID)
    ereport(ERROR,
            (errcode(ERRCODE_WRONG_OBJECT_TYPE),
             errmsg("function %s must return type %s",
                    NameListToString((List *)handler->arg), "fdw_handler")));

  return handler_oid;
}

/*
 * Given a validator `DefElem` returns its oid.
 */
static Oid lookup_fdw_validator_func(DefElem *validator) {
  if (validator == NULL || validator->arg == NULL) return InvalidOid;

  /* validators take text[], oid */
  Oid arg_types[2] = {TEXTARRAYOID, OIDOID};

  return LookupFuncName((List *)validator->arg, 2, arg_types, false);
  /* validator's return value is ignored, so we don't check the type */
}

/*
 * Parses function options from a `create foreign data wrapper ...` command
 * and returns the handler and validator functions' oids. If a function is
 * not specified in the command, returns InvalidOid for that function. This
 * function is not as strict as the one in Postgres for simplicity. The one
 * in Postgres does not allow duplidate handler or validator options while
 * this one returns the value of the last specified option in case of
 * duplicates. This is fine because such a command will be rejected anyways
 * by Postgres.
 */
static void parse_func_options(List *func_options, Oid *fdw_handler_oid,
                               Oid *fdw_validator_oid, DefElem **handler,
                               DefElem **validator) {
  ListCell *cell;
  *fdw_handler_oid   = InvalidOid;
  *fdw_validator_oid = InvalidOid;

  foreach (cell, func_options) {
    DefElem *def = (DefElem *)lfirst(cell);
    if (strcmp(def->defname, "handler") == 0) {
      *fdw_handler_oid = lookup_fdw_handler_func(def);
      *handler         = def;
    } else if (strcmp(def->defname, "validator") == 0) {
      *fdw_validator_oid = lookup_fdw_validator_func(def);
      *validator         = def;
    } else {
      continue;
    }
  }
}

/*
 * Returns a `Node` of two strings containing the fully qualfied name
 * of the function with `funcOid`.
 */
static Node *get_qualified_func(Oid funcOid) {
  Assert(OidIsValid(funcOid));

  Oid   namespace_oid = get_func_namespace(funcOid);
  char *schema_name   = get_namespace_name(namespace_oid);
  char *func_name     = get_func_name(funcOid);

  // If the function is dropped concurrently we don't want
  // to assign nulls to the list
  if (schema_name == NULL || func_name == NULL)
    ereport(ERROR, (errcode(ERRCODE_UNDEFINED_FUNCTION),
                    errmsg("function with oid %u no longer exists", funcOid)));

  return (Node *)list_make2(makeString(schema_name), makeString(func_name));
}

/*
 * Validates options given to the `create foreign data wrapper ...` sql command
 * by ensuring that:
 *
 * 1. The command specified both the validator and the handler functions.
 * 2. The validator and the handler functions are both owned by the same
 * extension.
 * 3. The owning extension is not a pg_tle extension.
 *
 * Also rewrites handler and validator names to be fully schema-qualified
 * so that Postgres does not re-resolve them to a different function later.
 *
 * All of the above measures ensure that an attacker cannot fool supautils or
 * Postgres into running their code as superuser.
 *
 * Forcing users to specify both validator and handler functions looks like
 * an onerous condition but it's backwards compatible on the Supabase
 * platform because we only have postgres_fdw, file_fdw or the wrappers
 * framework wrappers on our hosted projects. All three of them always needed
 * both handler and validator function to be specified.
 */
void verify_fdw_functions_ownership(List *func_options) {
  Oid      fdw_handler_oid;
  Oid      fdw_validator_oid;
  DefElem *handler   = NULL;
  DefElem *validator = NULL;

  parse_func_options(func_options, &fdw_handler_oid, &fdw_validator_oid,
                     &handler, &validator);

  if (OidIsValid(fdw_handler_oid)) {
    if (!is_func_owned_by_non_tle_extension(fdw_handler_oid)) {
      ereport(
          ERROR,
          (errcode(ERRCODE_INSUFFICIENT_PRIVILEGE),
           errmsg("\"%s\" must be a function owned by an extension "
                  "which is not a Trusted Language Extension, to use "
                  "in foreign-data wrapper's %s option",
                  NameListToString((List *)handler->arg), handler->defname)));
    }

    handler->arg = get_qualified_func(fdw_handler_oid);
  }

  if (OidIsValid(fdw_validator_oid)) {
    if (!is_func_owned_by_non_tle_extension(fdw_validator_oid)) {
      ereport(ERROR,
              (errcode(ERRCODE_INSUFFICIENT_PRIVILEGE),
               errmsg("\"%s\" must be a function owned by an extension "
                      "which is not a Trusted Language Extension, to use "
                      "in foreign-data wrapper's %s option",
                      NameListToString((List *)validator->arg),
                      validator->defname)));
    }

    validator->arg = get_qualified_func(fdw_validator_oid);
  }
}
