#ifndef PRIVILEGED_ROLE_H
#define PRIVILEGED_ROLE_H

#include "pg_prelude.h"

// What the privileged role handlers need from the configuration.
typedef struct {
  const char *superuser;
  const char *privileged_role;
  const char *allowed_configs;
} privileged_role_policy;

/**
 * Handles what `supautils.privileged_role` may do beyond its own privileges:
 * create foreign data wrappers, publications and event triggers (which it then
 * owns), alter publications, and set the allowed configs.
 *
 * Returns true when the statement was run, false when the hook should carry
 * on with its normal processing.
 */
extern bool handle_privileged_role_stmt(Node *stmt, const utility_call *call,
                                        const privileged_role_policy *policy);

#endif
