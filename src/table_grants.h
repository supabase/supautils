#ifndef TABLE_GRANTS_H
#define TABLE_GRANTS_H

#include "pg_prelude.h"

#include "drop_trigger_grants.h"
#include "policy_grants.h"

// What the table grant handlers need from the configuration.
typedef struct {
  const char                *superuser;
  const policy_grants       *policy_grants;
  size_t                     total_policy_grants;
  const drop_trigger_grants *drop_trigger_grants;
  size_t                     total_drop_trigger_grants;
} table_grant_policy;

/**
 * Handles the statements a role may run on tables it doesn't own because of
 * `supautils.policy_grants` and `supautils.drop_trigger_grants`: CREATE, ALTER,
 * DROP and COMMENT ON POLICY, and DROP TRIGGER.
 *
 * Returns true when the statement was run, false when the hook should carry
 * on with its normal processing.
 */
extern bool handle_table_grant_stmt(Node *stmt, const utility_call *call,
                                    const table_grant_policy *policy);

#endif
