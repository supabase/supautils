#ifndef ROLES_H
#define ROLES_H

#include "pg_prelude.h"

// What the role handlers need from the configuration.
typedef struct {
  const char *superuser;
  const char *privileged_role;
  const char *reserved_roles;
  const char *reserved_memberships;
  const char *reserved_role_members;
  const char *privileged_role_allowed_configs;
} role_policy;

/**
 * Whether `target` is one of the reserved roles. Roles listed with a trailing
 * `*` are configurable: they still count as reserved unless
 * `allow_configurable_roles` is set. The roots in `reserved_role_members` and
 * their transitive members are always reserved.
 */
extern bool is_reserved_role(const char *target, bool allow_configurable_roles,
                             const char *reserved_roles,
                             const char *reserved_role_members);

/**
 * Handles the role DDL that supautils intercepts: CREATE, ALTER, ALTER ... SET,
 * DROP, RENAME and GRANT/REVOKE of roles.
 *
 * Returns true when the statement was run, false when the hook should carry
 * on with its normal processing. Reserved roles and memberships raise an error
 * in either case.
 */
extern bool handle_role_stmt(Node *stmt, const utility_hook_args *args,
                             const role_policy *policy);

#endif
