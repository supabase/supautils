#include "table_grants.h"

#include "utils.h"

// The table a policy or trigger belongs to: all but the last element of the
// qualified object name.
static RangeVar *object_table(List *object) {
  List *table_name_list =
      list_truncate(list_copy(object), list_length(object) - 1);
  return makeRangeVarFromNameList(table_name_list);
}

static bool policy_granted(const RangeVar *table, LOCKMODE lockmode,
                           const table_grant_policy *policy) {
  return is_current_role_granted_table_policy(
      table, policy->policy_grants, policy->total_policy_grants, lockmode);
}

/*
 * CREATE POLICY / ALTER POLICY
 */
static bool alter_policy(RangeVar *table, const utility_call *call,
                         const table_grant_policy *policy) {
  if (superuser()) {
    return false;
  }
  if (!policy_granted(table, AccessExclusiveLock, policy)) {
    return false;
  }

  RUN_ELEVATED(policy->superuser, run_prev_utility_hook(call));

  return true;
}

/*
 * DROP POLICY / DROP TRIGGER
 */
static bool drop_policy_or_trigger(DropStmt *stmt, const utility_call *call,
                                   const table_grant_policy *policy) {
  if (stmt->removeType != OBJECT_POLICY && stmt->removeType != OBJECT_TRIGGER) {
    return false;
  }
  if (superuser()) {
    return false;
  }

  // DROP POLICY and DROP TRIGGER always have one object.
  RangeVar *table =
      object_table(castNode(List, lfirst(list_head(stmt->objects))));

  if (stmt->removeType == OBJECT_POLICY) {
    if (!policy_granted(table, AccessExclusiveLock, policy)) {
      return false;
    }
  } else {
    if (!is_current_role_granted_table_drop_trigger(
            table, policy->drop_trigger_grants,
            policy->total_drop_trigger_grants)) {
      return false;
    }
  }

  RUN_ELEVATED(policy->superuser, run_prev_utility_hook(call));

  return true;
}

/*
 * COMMENT ON POLICY
 */
static bool comment_on_policy(CommentStmt *stmt, const utility_call *call,
                              const table_grant_policy *policy) {
  if (stmt->objtype != OBJECT_POLICY) {
    return false;
  }
  if (!IsTransactionState()) {
    return false;
  }
  if (superuser()) {
    return false;
  }

  RangeVar *table = object_table(castNode(List, stmt->object));

  if (!policy_granted(table, AccessShareLock, policy)) {
    return false;
  }

  RUN_ELEVATED(policy->superuser, run_prev_utility_hook(call));

  return true;
}

bool handle_table_grant_stmt(Node *stmt, const utility_call *call,
                             const table_grant_policy *policy) {
  switch (nodeTag(stmt)) {
  case T_CreatePolicyStmt:
    return alter_policy(((CreatePolicyStmt *)stmt)->table, call, policy);
  case T_AlterPolicyStmt:
    return alter_policy(((AlterPolicyStmt *)stmt)->table, call, policy);
  case T_DropStmt:
    return drop_policy_or_trigger((DropStmt *)stmt, call, policy);
  case T_CommentStmt:
    return comment_on_policy((CommentStmt *)stmt, call, policy);
  default: return false;
  }
}
