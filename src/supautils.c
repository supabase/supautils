#include "pg_prelude.h"

#include "constrained_extensions.h"
#include "drop_trigger_grants.h"
#include "event_triggers.h"
#include "extension_custom_scripts.h"
#include "extensions.h"
#include "extensions_parameter_overrides.h"
#include "fdw.h"
#include "permission_hints.h"
#include "policy_grants.h"
#include "privileged_extensions.h"
#include "privileged_role.h"
#include "roles.h"
#include "table_grants.h"
#include "timezone.h"

#define EREPORT_INVALID_PARAMETER(name)                                        \
  ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE),                    \
                  errmsg("parameter \"%s\" must be a comma-separated list of " \
                         "identifiers",                                        \
                         name)));

#if PG_VERSION_NUM >= 180000
PG_MODULE_MAGIC_EXT(.name = "supautils", .version = MODVERSION);
#else
PG_MODULE_MAGIC;
#endif

static char *reserved_roles                 = NULL;
static char *reserved_memberships           = NULL;
static char *placeholders                   = NULL;
static char *placeholders_disallowed_values = NULL;
static char *empty_placeholder              = NULL;
static char *privileged_extensions          = NULL;
static char *supautils_superuser            = NULL;
static char *extension_custom_scripts_path  = NULL;
static char *privileged_role = NULL; // the privileged_role is a proxy role for
                                     // the `supautils.superuser` role
static char *privileged_role_allowed_configs = NULL;
static char *hint_roles                      = NULL;

static ProcessUtility_hook_type prev_hook                = NULL;
static fmgr_hook_type           next_fmgr_hook           = NULL;
static needs_fmgr_hook_type     next_needs_fmgr_hook     = NULL;
static ExecutorStart_hook_type  prev_executor_start_hook = NULL;

static char                 *constrained_extensions_str        = NULL;
static constrained_extension cexts[MAX_CONSTRAINED_EXTENSIONS] = {0};
static size_t                total_cexts                       = 0;

static char                         *extensions_parameter_overrides_str = NULL;
static extension_parameter_overrides epos[MAX_EXTENSIONS_PARAMETER_OVERRIDES] =
    {0};
static size_t total_epos = 0;

static char         *policy_grants_str      = NULL;
static policy_grants pgs[MAX_POLICY_GRANTS] = {0};
static size_t        total_pgs              = 0;

static char               *drop_trigger_grants_str       = NULL;
static drop_trigger_grants dtgs[MAX_DROP_TRIGGER_GRANTS] = {0};
static size_t              total_dtgs                    = 0;

static bool log_skipped_evtrigs = false;
static bool disable_program     = false;

static const struct config_enum_entry restrict_extension_versions_options[] = {
  {"off", RESTRICT_EXTENSION_VERSIONS_OFF, false},
  {"warn", RESTRICT_EXTENSION_VERSIONS_WARN, false},
  {"error", RESTRICT_EXTENSION_VERSIONS_ERROR, false},
  {NULL, 0, false}};

static int restrict_extension_versions = RESTRICT_EXTENSION_VERSIONS_OFF;

void _PG_init(void);
void _PG_fini(void);

static bool is_hint_role(const char *target);

static void check_parameter(char *val, char *name);

static void supautils_executor_start(QueryDesc *queryDesc, int eflags);

// the hook will only be attached to functions that `RETURN event_trigger`
static bool supautils_needs_fmgr_hook(Oid functionId) {
  if (next_needs_fmgr_hook && (*next_needs_fmgr_hook)(functionId)) return true;

  return is_event_trigger_function(functionId);
}

static void skip_event_trigger(FmgrInfo *flinfo, const char *func_name,
                               const char *current_role_name,
                               const char *role_descriptor,
                               const char *function_condition,
                               const char *owner_name) {
  if (log_skipped_evtrigs) {
    ereport(NOTICE,
            errmsg("Skipping event trigger function \"%s\" for user \"%s\"",
                   func_name, current_role_name),
            errdetail("\"%s\" is %s and the function \"%s\" %s \"%s\"",
                      current_role_name, role_descriptor, func_name,
                      function_condition, owner_name));
  }

  // we can't skip execution directly inside the fmgr_hook (although we can
  // abort it with ereport) so instead we use the workaround of changing the
  // function to a noop function
  force_noop(flinfo);
}

// This function will fire twice: once before execution of the database function
// (event=FHET_START) and once after execution has finished or failed
// (event=FHET_END/FHET_ABORT).
static void supautils_fmgr_hook(FmgrHookEventType event, FmgrInfo *flinfo,
                                Datum *private) {
  switch (event) {
  // we only need to change behavior before the function gets executed
  case FHET_START: {
    if (is_event_trigger_function(
            flinfo->fn_oid)) { // recheck the function is an event trigger in
                               // case another extension need_fmgr_hook passed
                               // our supautils_needs_fmgr_hook
      func_attrs fattrs = get_function_attrs(
          (func_search){.as = FO_SEARCH_FINFO, .val.finfo = flinfo});
      const Oid current_role_oid =
          fattrs.is_security_definer
              ?
              // when the function is security definer, we need to get the
              // session user id otherwise it will fire for superusers or
              // reserved roles. See
              // https://github.com/supabase/supautils/issues/140.
              GetOuterUserId()
              : GetUserId();
      const char *current_role_name =
          GetUserNameFromId(current_role_oid, false);
      const bool role_is_super = superuser_arg(current_role_oid);
      const bool role_is_reserved =
          is_reserved_role(current_role_name, false, reserved_roles);
      const bool  function_is_owned_by_super = superuser_arg(fattrs.owner);
      const bool  role_is_function_owner     = current_role_oid == fattrs.owner;
      const char *func_name                  = get_func_name(flinfo->fn_oid);
      const char *function_owner_name = GetUserNameFromId(fattrs.owner, false);
      if (role_is_super) {
        if (!function_is_owned_by_super) {
          skip_event_trigger(
              flinfo, func_name, current_role_name, "a superuser",
              "is not superuser-owned, it's owned by", function_owner_name);
        } else if (!role_is_function_owner) {
          skip_event_trigger(flinfo, func_name, current_role_name,
                             "a superuser",
                             "is not owned by the same role, it's owned by",
                             function_owner_name);
        }
      } else if (role_is_reserved) {
        if (!function_is_owned_by_super) {
          skip_event_trigger(
              flinfo, func_name, current_role_name, "a reserved role",
              "is not superuser-owned, it's owned by", function_owner_name);
        }
      }
    }

    if (next_fmgr_hook) (*next_fmgr_hook)(event, flinfo, private);
    break;
  }

  // do nothing when the function already executed
  case FHET_END:
  case FHET_ABORT:
    if (next_fmgr_hook) (*next_fmgr_hook)(event, flinfo, private);
    break;

  default: elog(ERROR, "unexpected event type: %d", (int)event); break;
  }
}

static void supautils_executor_start(QueryDesc *queryDesc, int eflags) {
  MemoryContext cur_ctx = CurrentMemoryContext;

  if (!is_hint_role(GetUserNameFromId(GetUserId(), false))) {
    if (prev_executor_start_hook)
      prev_executor_start_hook(queryDesc, eflags);
    else
      standard_ExecutorStart(queryDesc, eflags);
  } else {
    PG_TRY();
    {
      if (prev_executor_start_hook)
        prev_executor_start_hook(queryDesc, eflags);
      else
        standard_ExecutorStart(queryDesc, eflags);
    }
    PG_CATCH();
    {
      MemoryContext oldcxt = MemoryContextSwitchTo(cur_ctx);
      ErrorData    *edata  = CopyErrorData();
      MemoryContextSwitchTo(oldcxt);

      FlushErrorState();

      if (edata->sqlerrcode == ERRCODE_INSUFFICIENT_PRIVILEGE) {
        const Oid current_role_oid = GetUserId();

        missing_perm missing =
            find_missing_perm(queryDesc->plannedstmt, current_role_oid);

        if (missing.acl != 0 && OidIsValid(missing.relid) &&
            (missing.acl & (ACL_TRUNCATE | ACL_TRIGGER | ACL_REFERENCES)) ==
                0) {

          StringInfo privileges_str = makeStringInfo();
          build_privileges_string(privileges_str, missing.acl);

          if (privileges_str->len > 0) {
            char *schema = get_namespace_name(get_rel_namespace(missing.relid));
            char *relname = get_rel_name(missing.relid);

            if (relname != NULL) {
              char *qualified_rel_name =
                  quote_qualified_identifier(schema, relname);
              char *username = GetUserNameFromId(current_role_oid, false);
              char *quoted_role_name =
                  quote_qualified_identifier(NULL, username);

              edata->hint = psprintf(
                  "Grant the required privileges to the current "
                  "role with: GRANT %s ON %s TO %s;",
                  privileges_str->data, qualified_rel_name, quoted_role_name);
            }
          }
        }
      }

      ReThrowError(edata);
    }
    PG_END_TRY();
  }
}

static void supautils_hook_internal(PROCESS_UTILITY_PARAMS);

static void supautils_hook(PROCESS_UTILITY_PARAMS) {
  // A `return` or `break` out of a RUN_AS() body would skip its PG_END_TRY and
  // leave a dangling entry on the exception stack. Catch that in assert builds,
  // naming the statement so the offending arm is obvious.
  sigjmp_buf *exception_stack_at_entry PG_USED_FOR_ASSERTS_ONLY =
      PG_exception_stack;

  supautils_hook_internal(PROCESS_UTILITY_ARGS);

#ifdef USE_ASSERT_CHECKING
  if (PG_exception_stack != exception_stack_at_entry) {
    elog(PANIC, "supautils: exception stack leaked while processing %s",
         GetCommandTagName(CreateCommandTag(pstmt->utilityStmt)));
  }
#endif
}

static void supautils_hook_internal(PROCESS_UTILITY_PARAMS) {
  /* Get the utility statement from the planned statement */
  Node *utility_stmt = pstmt->utilityStmt;

  const utility_hook_args args = UTILITY_HOOK_ARGS(prev_hook);

  const extension_policy ext_policy = {
    .superuser             = supautils_superuser,
    .privileged_role       = privileged_role,
    .privileged_extensions = privileged_extensions,
    .custom_scripts_path   = extension_custom_scripts_path,
    .constrained           = cexts,
    .total_constrained     = total_cexts,
    .overrides             = epos,
    .total_overrides       = total_epos,
    .restrict_versions     = restrict_extension_versions,
  };

  const role_policy roles = {
    .superuser                       = supautils_superuser,
    .privileged_role                 = privileged_role,
    .reserved_roles                  = reserved_roles,
    .reserved_memberships            = reserved_memberships,
    .privileged_role_allowed_configs = privileged_role_allowed_configs,
  };

  const table_grant_policy table_grants = {
    .superuser                 = supautils_superuser,
    .policy_grants             = pgs,
    .total_policy_grants       = total_pgs,
    .drop_trigger_grants       = dtgs,
    .total_drop_trigger_grants = total_dtgs,
  };

  const privileged_role_policy privileged = {
    .superuser       = supautils_superuser,
    .privileged_role = privileged_role,
    .allowed_configs = privileged_role_allowed_configs,
  };

  if (handle_extension_stmt(utility_stmt, &args, &ext_policy)) return;
  if (handle_role_stmt(utility_stmt, &args, &roles)) return;
  if (handle_table_grant_stmt(utility_stmt, &args, &table_grants)) return;
  if (handle_privileged_role_stmt(utility_stmt, &args, &privileged)) return;

  /* Chain to previously defined hooks */
  run_prev_utility_hook(&args);
}

static void clear_extensions_parameter_overrides_array(
    extension_parameter_overrides *target, size_t count) {
  for (size_t i = 0; i < count; i++) {
    if (target[i].name != NULL) pfree(target[i].name);
    if (target[i].schema != NULL) pfree(target[i].schema);
  }
  memset(target, 0, sizeof(extension_parameter_overrides) * count);
}

static void clear_extensions_parameter_overrides(void) {
  clear_extensions_parameter_overrides_array(
      epos, MAX_EXTENSIONS_PARAMETER_OVERRIDES);
  total_epos = 0;
}

static bool extensions_parameter_overrides_check_hook(
    char **newval, __attribute__((unused)) void **extra,
    __attribute__((unused)) GucSource source) {
  extension_parameter_overrides tmp_epos[MAX_EXTENSIONS_PARAMETER_OVERRIDES] = {
    0};

  if (*newval) {
    json_extension_parameter_overrides_parse_state state =
        parse_extensions_parameter_overrides(*newval, tmp_epos);

    clear_extensions_parameter_overrides_array(
        tmp_epos, MAX_EXTENSIONS_PARAMETER_OVERRIDES);

    if (state.error_msg) {
      ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
                      errmsg("supautils.extensions_parameter_overrides: %s",
                             state.error_msg)));
    }
  }

  return true;
}

static void extensions_parameter_overrides_assign_hook(
    const char *newval, __attribute__((unused)) void *extra) {
  clear_extensions_parameter_overrides();

  if (newval) {
    json_extension_parameter_overrides_parse_state state =
        parse_extensions_parameter_overrides(newval, epos);
    if (state.error_msg) {
      ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
                      errmsg("supautils.extensions_parameter_overrides: %s",
                             state.error_msg)));
    }
    total_epos = state.total_epos;
  }
}

static bool policy_grants_check_hook(char                            **newval,
                                     __attribute__((unused)) void    **extra,
                                     __attribute__((unused)) GucSource source) {
  char *val = *newval;

  for (size_t i = 0; i < total_pgs; i++) {
    pfree(pgs[i].role_name);
    for (size_t j = 0; j < pgs[i].total_tables; j++) {
      pfree(pgs[i].table_names[j]);
    }
    pgs[i].total_tables = 0;
  }
  total_pgs = 0;

  if (val) {
    json_policy_grants_parse_state state = parse_policy_grants(val, pgs);
    if (state.error_msg) {
      ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
                      errmsg("supautils.policy_grants: %s", state.error_msg)));
    }
    total_pgs = state.total_pgs;
  }

  return true;
}

static bool
drop_trigger_grants_check_hook(char                            **newval,
                               __attribute__((unused)) void    **extra,
                               __attribute__((unused)) GucSource source) {
  char *val = *newval;

  for (size_t i = 0; i < total_dtgs; i++) {
    pfree(dtgs[i].role_name);
    for (size_t j = 0; j < dtgs[i].total_tables; j++) {
      pfree(dtgs[i].table_names[j]);
    }
    dtgs[i].total_tables = 0;
  }
  total_dtgs = 0;

  if (val) {
    json_drop_trigger_grants_parse_state state =
        parse_drop_trigger_grants(val, dtgs);
    if (state.error_msg) {
      ereport(ERROR,
              (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
               errmsg("supautils.drop_trigger_grants: %s", state.error_msg)));
    }
    total_dtgs = state.total_dtgs;
  }

  return true;
}

static bool
reserved_roles_check_hook(char **newval, __attribute__((unused)) void **extra,
                          __attribute__((unused)) GucSource source) {
  check_parameter(*newval, "supautils.reserved_roles");

  return true;
}

static bool
reserved_memberships_check_hook(char                            **newval,
                                __attribute__((unused)) void    **extra,
                                __attribute__((unused)) GucSource source) {
  check_parameter(*newval, "supautils.reserved_memberships");

  return true;
}

static bool placeholders_disallowed_values_check_hook(
    char **newval, __attribute__((unused)) void **extra,
    __attribute__((unused)) GucSource source) {
  check_parameter(*newval, "supautils.placeholders_disallowed_values");

  return true;
}

static bool
privileged_extensions_check_hook(char                            **newval,
                                 __attribute__((unused)) void    **extra,
                                 __attribute__((unused)) GucSource source) {
  check_parameter(*newval, "supautils.privileged_extensions");

  return true;
}

static bool hint_roles_check_hook(char                            **newval,
                                  __attribute__((unused)) void    **extra,
                                  __attribute__((unused)) GucSource source) {
  check_parameter(*newval, "supautils.hint_roles");

  return true;
}

static bool privileged_role_allowed_configs_check_hook(
    char **newval, __attribute__((unused)) void **extra,
    __attribute__((unused)) GucSource source) {
  check_parameter(*newval, "supautils.privileged_role_allowed_configs");

  return true;
}

static void check_parameter(char *val, char *name) {
  List *comma_separated_list;

  if (val != NULL) {
    if (!SplitIdentifierString(pstrdup(val), ',', &comma_separated_list))
      EREPORT_INVALID_PARAMETER(name);

    list_free(comma_separated_list);
  }
}

static void clear_constrained_extensions(void) {
  if (total_cexts > 0) {
    for (size_t i = 0; i < total_cexts; i++) {
      pfree(cexts[i].name);
    }
  }
  memset(cexts, 0, sizeof(cexts));
  total_cexts = 0;
}

static bool
constrained_extensions_check_hook(char                            **newval,
                                  __attribute__((unused)) void    **extra,
                                  __attribute__((unused)) GucSource source) {
  constrained_extension tmp_cexts[MAX_CONSTRAINED_EXTENSIONS] = {0};

  if (*newval) {
    json_constrained_extension_parse_state state =
        parse_constrained_extensions(*newval, tmp_cexts);

    for (int i = 0; i < state.total_cexts; i++) {
      pfree(tmp_cexts[i].name);
    }

    if (state.error_msg) {
      ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
                      errmsg("supautils.constrained_extensions: %s",
                             state.error_msg)));
    }
  }

  return true;
}

static void
constrained_extensions_assign_hook(const char                   *newval,
                                   __attribute__((unused)) void *extra) {
  clear_constrained_extensions();

  if (newval) {
    json_constrained_extension_parse_state state =
        parse_constrained_extensions(newval, cexts);
    if (state.error_msg) {
      ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
                      errmsg("supautils.constrained_extensions: %s",
                             state.error_msg)));
    }
    total_cexts = state.total_cexts;
  }
}

static bool is_hint_role(const char *target) {
  List     *hint_roles_list;
  ListCell *role;

#if TEST_CORE // this is only added during testing
  return true;
#endif

  if (hint_roles == NULL || target == NULL) {
    return false;
  }

  SplitIdentifierString(pstrdup(hint_roles), ',', &hint_roles_list);

  foreach (role, hint_roles_list) {
    char *configured_role = (char *)lfirst(role);

    if (strcmp(target, configured_role) == 0) {
      list_free(hint_roles_list);
      return true;
    }
  }

  list_free(hint_roles_list);

  return false;
}

static bool placeholders_check_hook(char                            **newval,
                                    __attribute__((unused)) void    **extra,
                                    __attribute__((unused)) GucSource source) {
  char *val = *newval;

  if (val) {
    List     *comma_separated_list;
    ListCell *cell;
    bool      saw_sep = false;

    if (!SplitIdentifierString(pstrdup(val), ',', &comma_separated_list))
      EREPORT_INVALID_PARAMETER("supautils.placeholders");

    foreach (cell, comma_separated_list) {
      for (const char *p = lfirst(cell); *p; p++) {
        // check if the GUC has a "." in it(if it's a placeholder)
        if (*p == GUC_QUALIFIER_SEPARATOR) saw_sep = true;
      }
    }

    list_free(comma_separated_list);

    if (!saw_sep)
      ereport(ERROR,
              (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
               errmsg("supautils.placeholders must contain guc placeholders")));
  }

  return true;
}

static bool
restrict_placeholders_check_hook(char                            **newval,
                                 __attribute__((unused)) void    **extra,
                                 __attribute__((unused)) GucSource source) {
  bool not_empty = placeholders_disallowed_values &&
                   placeholders_disallowed_values[0] != '\0';

  if (*newval && not_empty) {
    char *token, *string, *tofree;
    char *val = str_tolower(*newval, strlen(*newval), DEFAULT_COLLATION_OID);

    tofree = string = pstrdup(placeholders_disallowed_values);

    while ((token = strsep(&string, ",")) != NULL) {
      if (strstr(val, token)) {
        GUC_check_errcode(ERRCODE_INVALID_PARAMETER_VALUE);
        GUC_check_errmsg("The placeholder contains the \"%s\" disallowed value",
                         token);
        pfree(tofree);
        pfree(val);
        return false;
      }
    }

    pfree(tofree);
    pfree(val);
  }

  return true;
}

void _PG_init(void) {

  // process utility hook
  prev_hook           = ProcessUtility_hook;
  ProcessUtility_hook = supautils_hook;

  // fmgr hook
  next_needs_fmgr_hook = needs_fmgr_hook;
  needs_fmgr_hook      = supautils_needs_fmgr_hook;

  next_fmgr_hook = fmgr_hook;
  fmgr_hook      = supautils_fmgr_hook;

  prev_executor_start_hook = ExecutorStart_hook;
  ExecutorStart_hook       = supautils_executor_start;

  hook_timezone_check();
  check_timezone_at_startup();

  DefineCustomStringVariable("supautils.extensions_parameter_overrides",
                             "Overrides for CREATE EXTENSION parameters", NULL,
                             &extensions_parameter_overrides_str, NULL,
                             PGC_SIGHUP, 0,
                             &extensions_parameter_overrides_check_hook,
                             &extensions_parameter_overrides_assign_hook, NULL);

  DefineCustomStringVariable(
      "supautils.reserved_roles",
      "Comma-separated list of roles that cannot be modified", NULL,
      &reserved_roles, NULL, PGC_SIGHUP, 0, reserved_roles_check_hook, NULL,
      NULL);

  DefineCustomStringVariable(
      "supautils.reserved_memberships",
      "Comma-separated list of roles whose memberships cannot be granted", NULL,
      &reserved_memberships, NULL, PGC_SIGHUP, 0,
      reserved_memberships_check_hook, NULL, NULL);

  DefineCustomStringVariable(
      "supautils.placeholders",
      "GUC placeholders which will get values disallowed according to "
      "supautils.placeholders_disallowed_values",
      NULL, &placeholders, NULL, PGC_SIGHUP, 0, placeholders_check_hook, NULL,
      NULL);

  DefineCustomStringVariable(
      "supautils.placeholders_disallowed_values",
      "disallowed values for the GUC placeholders defined in "
      "supautils.placeholders",
      NULL, &placeholders_disallowed_values, NULL, PGC_SIGHUP, 0,
      placeholders_disallowed_values_check_hook, NULL, NULL);

  DefineCustomStringVariable("supautils.privileged_extensions",
                             "Comma-separated list of extensions which get "
                             "installed using supautils.superuser",
                             NULL, &privileged_extensions, NULL, PGC_SIGHUP, 0,
                             privileged_extensions_check_hook, NULL, NULL);

  DefineCustomStringVariable(
      "supautils.privileged_extensions_custom_scripts_path",
      "Path to load privileged extensions' custom scripts from. Deprecated: "
      "use supautils.extension_custom_scripts_path instead.",
      NULL, &extension_custom_scripts_path, NULL, PGC_SIGHUP, 0, NULL, NULL,
      NULL);

  DefineCustomStringVariable("supautils.extension_custom_scripts_path",
                             "Path to load extension custom scripts from", NULL,
                             &extension_custom_scripts_path, NULL, PGC_SIGHUP,
                             0, NULL, NULL, NULL);

  DefineCustomStringVariable(
      "supautils.superuser",
      "Superuser to install extensions in supautils.privileged_extensions as",
      NULL, &supautils_superuser, NULL, PGC_SIGHUP, 0, NULL, NULL, NULL);

  // TODO emit a warning when this deprecated GUC is used
  DefineCustomStringVariable(
      "supautils.privileged_extensions_superuser",
      "Superuser to install extensions in supautils.privileged_extensions "
      "as. Deprecated: use supautils.superuser instead.",
      NULL, &supautils_superuser, NULL, PGC_SIGHUP, 0, NULL, NULL, NULL);

  DefineCustomStringVariable(
      "supautils.privileged_role",
      "Non-superuser role to be granted with some superuser privileges", NULL,
      &privileged_role, NULL, PGC_SIGHUP, 0, NULL, NULL, NULL);

  DefineCustomStringVariable(
      "supautils.privileged_role_allowed_configs",
      "Superuser-only configs that the privileged_role is allowed to configure",
      NULL, &privileged_role_allowed_configs, NULL, PGC_SIGHUP, 0,
      privileged_role_allowed_configs_check_hook, NULL, NULL);

  DefineCustomStringVariable(
      "supautils.hint_roles",
      "Comma-separated list of roles that receive enhanced permission hints",
      NULL, &hint_roles, NULL, PGC_SIGHUP, 0, hint_roles_check_hook, NULL,
      NULL);

  DefineCustomStringVariable("supautils.constrained_extensions",
                             "Extensions that require a minimum amount of "
                             "CPUs, memory and free disk to be installed",
                             NULL, &constrained_extensions_str, NULL,
                             PGC_SIGHUP, 0, constrained_extensions_check_hook,
                             constrained_extensions_assign_hook, NULL);

  DefineCustomStringVariable("supautils.drop_trigger_grants",
                             "Allow non-owners to drop triggers on tables",
                             NULL, &drop_trigger_grants_str, NULL, PGC_SIGHUP,
                             0, &drop_trigger_grants_check_hook, NULL, NULL);

  DefineCustomStringVariable("supautils.policy_grants",
                             "Allow non-owners to manage policies on tables",
                             NULL, &policy_grants_str, NULL, PGC_SIGHUP, 0,
                             &policy_grants_check_hook, NULL, NULL);

  DefineCustomEnumVariable(
      "supautils.restrict_extension_versions",
      "Restrict CREATE/ALTER EXTENSION version specification to superusers",
      "off: no restriction; warn: ignore the specified version with a warning "
      "and use the default version; error: reject the statement",
      &restrict_extension_versions, RESTRICT_EXTENSION_VERSIONS_OFF,
      restrict_extension_versions_options, PGC_SUSET, 0, NULL, NULL, NULL);

  DefineCustomBoolVariable("supautils.log_skipped_evtrigs",
                           "Log skipped event triggers with a NOTICE level",
                           NULL, &log_skipped_evtrigs, false, PGC_USERSET, 0,
                           NULL, NULL, NULL);

  // DO NOT USE; here for backward compat
  DefineCustomBoolVariable("supautils.disable_program", NULL, NULL,
                           &disable_program, false, PGC_SIGHUP,
                           GUC_SUPERUSER_ONLY, NULL, NULL, NULL);

  if (placeholders) {
    List     *comma_separated_list;
    ListCell *cell;

    SplitIdentifierString(pstrdup(placeholders), ',', &comma_separated_list);

    foreach (cell, comma_separated_list) {
      char *pholder = (char *)lfirst(cell);

      DefineCustomStringVariable(pholder, "", NULL, &empty_placeholder, NULL,
                                 PGC_USERSET, 0,
                                 restrict_placeholders_check_hook, NULL, NULL);
    }
    list_free(comma_separated_list);
  }

  EmitWarningsOnPlaceholders("supautils");
}

/*
 * This is just for completion. Right now postgres doesn't call _PG_fini, see:
 * https://github.com/postgres/postgres/blob/master/src/backend/utils/fmgr/dfmgr.c#L388-L402
 */
void _PG_fini(void) {
  ProcessUtility_hook = prev_hook;
  ExecutorStart_hook  = prev_executor_start_hook;
}
