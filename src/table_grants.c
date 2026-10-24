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
static bool create_or_alter_policy(const RangeVar           *table,
                                   const utility_hook_args  *args,
                                   const table_grant_policy *policy) {
  if (superuser()) {
    return false;
  }
  if (!policy_granted(table, AccessExclusiveLock, policy)) {
    return false;
  }

  RUN_ELEVATED(policy->superuser, run_prev_utility_hook(args));

  return true;
}

/*
 * DROP POLICY
 */
static bool drop_policy(const RangeVar *table, const utility_hook_args *args,
                        const table_grant_policy *policy) {
  if (superuser()) {
    return false;
  }

  if (table == NULL) {
    return false;
  }

  if (!policy_granted(table, AccessExclusiveLock, policy)) {
    return false;
  }

  RUN_ELEVATED(policy->superuser, run_prev_utility_hook(args));

  return true;
}

/*
 * DROP TRIGGER
 */
static bool drop_trigger(const RangeVar *table, const utility_hook_args *args,
                         const table_grant_policy *policy) {
  if (superuser()) {
    return false;
  }

  if (table == NULL) {
    return false;
  }

  if (!is_current_role_granted_table_drop_trigger(
          table, policy->drop_trigger_grants,
          policy->total_drop_trigger_grants)) {
    return false;
  }

  RUN_ELEVATED(policy->superuser, run_prev_utility_hook(args));

  return true;
}

/*
 * COMMENT ON POLICY
 */
static bool comment_on_policy(const RangeVar           *table,
                              const utility_hook_args  *args,
                              const table_grant_policy *policy) {
  if (!IsTransactionState()) {
    return false;
  }
  if (superuser()) {
    return false;
  }

  if (!policy_granted(table, AccessShareLock, policy)) {
    return false;
  }

  RUN_ELEVATED(policy->superuser, run_prev_utility_hook(args));

  return true;
}

/*
 * The table a DROP POLICY / DROP TRIGGER names, or NULL when the statement
 * doesn't carry exactly one object. The grammar always gives one; anything
 * else is left to core.
 */
static RangeVar *dropped_object_table(const DropStmt *stmt) {
  if (list_length(stmt->objects) != 1) return NULL;
  return object_table(castNode(List, linitial(stmt->objects)));
}

/*
 * Handlers never see a parse node: the switch below reads the few fields the
 * policy needs and hands over plain values.
 */
bool handle_table_grant_stmt(Node *stmt, const utility_hook_args *args,
                             const table_grant_policy *policy) {
  switch (nodeTag(stmt)) {
  case T_CreatePolicyStmt:
    return create_or_alter_policy(((CreatePolicyStmt *)stmt)->table, args,
                                  policy);
  case T_AlterPolicyStmt:
    return create_or_alter_policy(((AlterPolicyStmt *)stmt)->table, args,
                                  policy);
  case T_DropStmt: {
    DropStmt *drop = (DropStmt *)stmt;
    switch (drop->removeType) {
    case OBJECT_POLICY:
      return drop_policy(dropped_object_table(drop), args, policy);
    case OBJECT_TRIGGER:
      return drop_trigger(dropped_object_table(drop), args, policy);
    default: return false;
    }
  }
  case T_CommentStmt: {
    CommentStmt *comment = (CommentStmt *)stmt;
    if (comment->objtype != OBJECT_POLICY) return false;
    return comment_on_policy(object_table(castNode(List, comment->object)),
                             args, policy);
  }
  default: return false;
  }
}
