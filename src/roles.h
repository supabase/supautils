#ifndef ROLES_H
#define ROLES_H

#include "pg_prelude.h"

// What the role handlers need from the configuration.
typedef struct {
  const char *superuser;
  const char *privileged_role;
  const char *reserved_roles;
  const char *reserved_memberships;
  const char *privileged_role_allowed_configs;
} role_policy;

/**
 * Whether `target` is one of the reserved roles. Roles listed with a trailing
 * `*` are configurable: they still count as reserved unless
 * `allow_configurable_roles` is set.
 */
extern bool is_reserved_role(const char *target, bool allow_configurable_roles,
                             const char *reserved_roles);

/**
 * Handles the role DDL that supautils intercepts: CREATE, ALTER, ALTER ... SET,
 * DROP, RENAME and GRANT/REVOKE of roles.
 *
 * Returns true when the statement was run, false when the hook should carry
 * on with its normal processing. Reserved roles and memberships raise an error
 * in either case.
 */
extern bool handle_role_stmt(Node *stmt, const utility_call *call,
                             const role_policy *policy);

#endif
