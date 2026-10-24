#include "privileged_role.h"

#include "event_triggers.h"
#include "fdw.h"
#include "utils.h"

/*
 * CREATE FOREIGN DATA WRAPPER <fdw>
 */
static bool create_fdw(const char *fdwname, List *func_options,
                       const utility_hook_args      *args,
                       const privileged_role_policy *policy) {
  const Oid current_user_id = GetUserId();

  if (superuser()) {
    return false;
  }
  if (!is_current_role_privileged(policy->privileged_role)) {
    return false;
  }

  validate_func_options(func_options);

  RUN_ELEVATED(policy->superuser, run_prev_utility_hook(args);

               // Change FDW owner to the current role (which is a privileged
               // role)
               alter_owner(fdwname, current_user_id, ALT_FDW));

  return true;
}

/*
 * CREATE PUBLICATION
 */
static bool create_publication(const char                   *pubname,
                               const utility_hook_args      *args,
                               const privileged_role_policy *policy) {
  const Oid current_user_id = GetUserId();

  if (superuser()) {
    return false;
  }
  if (!is_current_role_privileged(policy->privileged_role)) {
    return false;
  }

  RUN_ELEVATED(policy->superuser, run_prev_utility_hook(args);

               // Change publication owner to the current role (which is a
               // privileged role)
               alter_owner(pubname, current_user_id, ALT_PUB));

  return true;
}

/*
 * ALTER PUBLICATION <name> ADD TABLES IN SCHEMA ...
 */
static bool alter_publication(const utility_hook_args      *args,
                              const privileged_role_policy *policy) {
  if (superuser()) {
    return false;
  }
  if (!is_current_role_privileged(policy->privileged_role)) {
    return false;
  }

  RUN_ELEVATED(policy->superuser, run_prev_utility_hook(args));

  return true;
}

/*
 * SET <allowed config> ...
 */
// config_name is NULL for RESET ALL.
static bool set_allowed_config(const char                   *config_name,
                               const utility_hook_args      *args,
                               const privileged_role_policy *policy) {
  if (!IsTransactionState()) {
    return false;
  }
  if (superuser()) {
    return false;
  }
  if (policy->allowed_configs == NULL) {
    return false;
  }
  if (!is_string_in_comma_delimited_string(config_name,
                                           policy->allowed_configs)) {
    return false;
  }
  if (!is_current_role_privileged(policy->privileged_role)) {
    return false;
  }

  RUN_ELEVATED(policy->superuser, run_prev_utility_hook(args));

  return true;
}

/*
 * CREATE EVENT TRIGGER
 */
static bool create_event_trigger(const char *trigname, List *funcname,
                                 const utility_hook_args      *args,
                                 const privileged_role_policy *policy) {
  if (!IsTransactionState()) {
    return false;
  }
  if (!is_current_role_privileged(policy->privileged_role)) {
    return false;
  }

  const Oid current_user_id = GetUserId();

  bool       current_user_is_super = superuser_arg(current_user_id);
  func_attrs fattrs =
      get_function_attrs((func_search){FO_SEARCH_NAME, {funcname}});
  bool function_is_owned_by_super = superuser_arg(fattrs.owner);

  if (!current_user_is_super && function_is_owned_by_super) {
    ereport(ERROR, (errmsg("Non-superuser owned event trigger must execute "
                           "a non-superuser owned function"),
                    errdetail("The current user \"%s\" is not a superuser "
                              "and the function \"%s\" is "
                              "owned by a superuser",
                              GetUserNameFromId(current_user_id, false),
                              NameListToString(funcname))));
  }

  if (current_user_is_super && !function_is_owned_by_super) {
    ereport(ERROR, (errmsg("Superuser owned event trigger must execute a "
                           "superuser owned function"),
                    errdetail("The current user \"%s\" is a superuser and "
                              "the function \"%s\" is "
                              "owned by a non-superuser",
                              GetUserNameFromId(current_user_id, false),
                              NameListToString(funcname))));
  }

  RUN_ELEVATED(
      policy->superuser, run_prev_utility_hook(args);

      if (!current_user_is_super) {
        // Change event trigger owner to the current role (which is a
        // privileged role)
        alter_owner(trigname, current_user_id, ALT_EVTRIG);
      });

  return true;
}

/*
 * Handlers never see a parse node: the switch below reads the few fields the
 * policy needs and hands over plain values.
 */
bool handle_privileged_role_stmt(Node *stmt, const utility_hook_args *args,
                                 const privileged_role_policy *policy) {
  switch (nodeTag(stmt)) {
  case T_CreateFdwStmt: {
    CreateFdwStmt *fdw = (CreateFdwStmt *)stmt;
    return create_fdw(fdw->fdwname, fdw->func_options, args, policy);
  }
  case T_CreatePublicationStmt:
    return create_publication(((CreatePublicationStmt *)stmt)->pubname, args,
                              policy);
  case T_AlterPublicationStmt: return alter_publication(args, policy);
  case T_VariableSetStmt:
    return set_allowed_config(((VariableSetStmt *)stmt)->name, args, policy);
  case T_CreateEventTrigStmt: {
    CreateEventTrigStmt *evt = (CreateEventTrigStmt *)stmt;
    return create_event_trigger(evt->trigname, evt->funcname, args, policy);
  }
  default: return false;
  }
}
