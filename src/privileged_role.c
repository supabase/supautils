#include "privileged_role.h"

#include "event_triggers.h"
#include "fdw.h"
#include "utils.h"

/*
 * CREATE FOREIGN DATA WRAPPER <fdw>
 */
static bool create_fdw(CreateFdwStmt *stmt, const utility_call *call,
                       const privileged_role_policy *policy) {
  const Oid current_user_id = GetUserId();

  if (superuser()) {
    return false;
  }
  if (!is_current_role_privileged(policy->privileged_role)) {
    return false;
  }

  validate_func_options(stmt->func_options);

  RUN_ELEVATED(policy->superuser, run_prev_utility_hook(call);

               // Change FDW owner to the current role (which is a privileged
               // role)
               alter_owner(stmt->fdwname, current_user_id, ALT_FDW));

  return true;
}

/*
 * CREATE PUBLICATION
 */
static bool create_publication(CreatePublicationStmt        *stmt,
                               const utility_call           *call,
                               const privileged_role_policy *policy) {
  const Oid current_user_id = GetUserId();

  if (superuser()) {
    return false;
  }
  if (!is_current_role_privileged(policy->privileged_role)) {
    return false;
  }

  RUN_ELEVATED(policy->superuser, run_prev_utility_hook(call);

               // Change publication owner to the current role (which is a
               // privileged role)
               alter_owner(stmt->pubname, current_user_id, ALT_PUB));

  return true;
}

/*
 * ALTER PUBLICATION <name> ADD TABLES IN SCHEMA ...
 */
static bool alter_publication(const utility_call           *call,
                              const privileged_role_policy *policy) {
  if (superuser()) {
    return false;
  }
  if (!is_current_role_privileged(policy->privileged_role)) {
    return false;
  }

  RUN_ELEVATED(policy->superuser, run_prev_utility_hook(call));

  return true;
}

/*
 * SET <allowed config> ...
 */
static bool set_allowed_config(VariableSetStmt *stmt, const utility_call *call,
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
  if (!is_string_in_comma_delimited_string(stmt->name,
                                           policy->allowed_configs)) {
    return false;
  }
  if (!is_current_role_privileged(policy->privileged_role)) {
    return false;
  }

  RUN_ELEVATED(policy->superuser, run_prev_utility_hook(call));

  return true;
}

/*
 * CREATE EVENT TRIGGER
 */
static bool create_event_trigger(CreateEventTrigStmt          *stmt,
                                 const utility_call           *call,
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
      get_function_attrs((func_search){FO_SEARCH_NAME, {stmt->funcname}});
  bool function_is_owned_by_super = superuser_arg(fattrs.owner);

  if (!current_user_is_super && function_is_owned_by_super) {
    ereport(ERROR, (errmsg("Non-superuser owned event trigger must execute "
                           "a non-superuser owned function"),
                    errdetail("The current user \"%s\" is not a superuser "
                              "and the function \"%s\" is "
                              "owned by a superuser",
                              GetUserNameFromId(current_user_id, false),
                              NameListToString(stmt->funcname))));
  }

  if (current_user_is_super && !function_is_owned_by_super) {
    ereport(ERROR, (errmsg("Superuser owned event trigger must execute a "
                           "superuser owned function"),
                    errdetail("The current user \"%s\" is a superuser and "
                              "the function \"%s\" is "
                              "owned by a non-superuser",
                              GetUserNameFromId(current_user_id, false),
                              NameListToString(stmt->funcname))));
  }

  RUN_ELEVATED(
      policy->superuser, run_prev_utility_hook(call);

      if (!current_user_is_super) {
        // Change event trigger owner to the current role (which is a
        // privileged role)
        alter_owner(stmt->trigname, current_user_id, ALT_EVTRIG);
      });

  return true;
}

bool handle_privileged_role_stmt(Node *stmt, const utility_call *call,
                                 const privileged_role_policy *policy) {
  switch (nodeTag(stmt)) {
  case T_CreateFdwStmt: return create_fdw((CreateFdwStmt *)stmt, call, policy);
  case T_CreatePublicationStmt:
    return create_publication((CreatePublicationStmt *)stmt, call, policy);
  case T_AlterPublicationStmt: return alter_publication(call, policy);
  case T_VariableSetStmt:
    return set_allowed_config((VariableSetStmt *)stmt, call, policy);
  case T_CreateEventTrigStmt:
    return create_event_trigger((CreateEventTrigStmt *)stmt, call, policy);
  default: return false;
  }
}
