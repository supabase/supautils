#include "roles.h"

#include "utils.h"

#define EREPORT_RESERVED_MEMBERSHIP(name)                                      \
  ereport(ERROR,                                                               \
          (errcode(ERRCODE_INSUFFICIENT_PRIVILEGE),                            \
           errmsg("\"%s\" role memberships are reserved, only superusers "     \
                  "can grant them",                                            \
                  name)))

#define EREPORT_RESERVED_ROLE(name)                                            \
  ereport(ERROR,                                                               \
          (errcode(ERRCODE_INSUFFICIENT_PRIVILEGE),                            \
           errmsg("\"%s\" is a reserved role, only superusers can modify "     \
                  "it",                                                        \
                  name)))

bool is_reserved_role(const char *target, bool allow_configurable_roles,
                      const char *reserved_roles) {
  List     *reserved_roles_list;
  ListCell *role;

  if (reserved_roles) {
    SplitIdentifierString(pstrdup(reserved_roles), ',', &reserved_roles_list);

    foreach (role, reserved_roles_list) {
      char *reserved_role        = (char *)lfirst(role);
      bool  is_configurable_role = remove_ending_wildcard(reserved_role);
      bool  should_modify_role =
          is_configurable_role && allow_configurable_roles;

      if (strcmp(target, reserved_role) == 0) {
        if (should_modify_role) {
          continue;
        } else {
          list_free(reserved_roles_list);
          return true;
        }
      }
    }
    list_free(reserved_roles_list);
  }

  return false;
}

static void confirm_reserved_memberships(const char *target,
                                         const char *reserved_memberships) {
  List     *reserved_memberships_list;
  ListCell *membership;

  if (reserved_memberships) {
    SplitIdentifierString(pstrdup(reserved_memberships), ',',
                          &reserved_memberships_list);

    foreach (membership, reserved_memberships_list) {
      char *reserved_membership = (char *)lfirst(membership);

      if (strcmp(target, reserved_membership) == 0) {
        list_free(reserved_memberships_list);
        EREPORT_RESERVED_MEMBERSHIP(reserved_membership);
      }
    }
    list_free(reserved_memberships_list);
  }
}

/*
 * ALTER ROLE <role> NOLOGIN NOINHERIT..
 */
static bool alter_role(AlterRoleStmt *stmt, const utility_hook_args *args,
                       const role_policy *policy) {
  ListCell *option_cell = NULL;

  if (!IsTransactionState()) {
    return false;
  }
  if (superuser()) {
    return false;
  }

  char *role_name = get_rolespec_name(stmt->role);

  if (is_reserved_role(role_name, false, policy->reserved_roles))
    EREPORT_RESERVED_ROLE(role_name);

  if (!is_current_role_privileged(policy->privileged_role)) {
    return false;
  }

  if (is_role_privileged(role_name, policy->privileged_role)) {
    ereport(ERROR, (errcode(ERRCODE_INSUFFICIENT_PRIVILEGE),
                    errmsg("permission denied to alter role"),
                    errdetail("Only superusers can alter privileged roles.")));
  }

  // Setting the superuser attribute is not allowed.
  foreach (option_cell, stmt->options) {
    DefElem *defel = lfirst_node(DefElem, option_cell);
    if (strcmp(defel->defname, "superuser") == 0) {
      ereport(ERROR, (errcode(ERRCODE_INSUFFICIENT_PRIVILEGE),
                      errmsg("permission denied to alter role"),
                      errdetail("Only roles with the %s attribute may alter "
                                "roles with the %s attribute.",
                                "SUPERUSER", "SUPERUSER")));
    }
  }

  // Allow setting bypassrls & replication.
  RUN_ELEVATED(policy->superuser, run_prev_utility_hook(args));

  return true;
}

/*
 * ALTER ROLE <role> SET search_path TO ...
 */
static bool alter_role_set(AlterRoleSetStmt        *stmt,
                           const utility_hook_args *args,
                           const role_policy       *policy) {
  bool role_is_privileged = false;

  if (!IsTransactionState()) {
    return false;
  }
  if (superuser()) {
    return false;
  }

  role_is_privileged = is_current_role_privileged(policy->privileged_role);

  char *role_name = get_rolespec_name(stmt->role);

  if (is_reserved_role(role_name, role_is_privileged, policy->reserved_roles))
    EREPORT_RESERVED_ROLE(role_name);

  if (!role_is_privileged) {
    return false;
  }

  if (policy->privileged_role_allowed_configs == NULL) {
    return false;
  }
  if (!is_string_in_comma_delimited_string(
          ((VariableSetStmt *)stmt->setstmt)->name,
          policy->privileged_role_allowed_configs)) {
    return false;
  }

  RUN_ELEVATED(policy->superuser, run_prev_utility_hook(args));

  return true;
}

/*
 * CREATE ROLE
 */
static bool create_role(CreateRoleStmt *stmt, const utility_hook_args *args,
                        const role_policy *policy) {
  const char *created_role   = stmt->role;
  List       *addroleto      = NIL;   /* roles to make this a member of */
  bool        hasrolemembers = false; /* has roles to be members of this role */
  ListCell   *option_cell;

  if (!IsTransactionState() || superuser()) {
    return false;
  }

  /* if role already exists, bypass the hook to let it fail with the usual
   * error */
  if (OidIsValid(get_role_oid(created_role, true))) return false;

  /* CREATE ROLE <reserved_role> */
  if (is_reserved_role(created_role, false, policy->reserved_roles))
    EREPORT_RESERVED_ROLE(created_role);

  /* Check to see if there are any descriptions related to membership. */
  foreach (option_cell, stmt->options) {
    DefElem *defel = lfirst_node(DefElem, option_cell);
    if (strcmp(defel->defname, "addroleto") == 0)
      addroleto = (List *)defel->arg;

    if (strcmp(defel->defname, "rolemembers") == 0 ||
        strcmp(defel->defname, "adminmembers") == 0)
      hasrolemembers = true;

    // Setting the superuser attribute is not allowed.
    if (strcmp(defel->defname, "superuser") == 0 && defGetBoolean(defel)) {
      ereport(ERROR, (errcode(ERRCODE_INSUFFICIENT_PRIVILEGE),
                      errmsg("permission denied to create role"),
                      errdetail("Only roles with the %s attribute may "
                                "create roles with the %s attribute.",
                                "SUPERUSER", "SUPERUSER")));
    }
  }

  /* CREATE ROLE <any_role> IN ROLE/GROUP <role_with_reserved_membership> */
  if (addroleto) {
    ListCell *role_cell;
    foreach (role_cell, addroleto) {
      RoleSpec *rolemember = lfirst_node(RoleSpec, role_cell);
      confirm_reserved_memberships(get_rolespec_name(rolemember),
                                   policy->reserved_memberships);
    }
  }

  /*
   * CREATE ROLE <role_with_reserved_membership> ROLE/ADMIN/USER <any_role>
   *
   * This is a contrived case because the "role_with_reserved_membership"
   * should already exist, but handle it anyway.
   */
  if (hasrolemembers)
    confirm_reserved_memberships(created_role, policy->reserved_memberships);

  // We don't want to switch to superuser on PG16+ because the
  // creating role is implicitly granted ADMIN on the new
  // role:
  // https://www.postgresql.org/docs/16/runtime-config-client.html#GUC-CREATEROLE-SELF-GRANT
  //
  // This ADMIN will be missing if we switch to superuser
  // since the creating role becomes the superuser.
  //
  // We also no longer need superuser to grant BYPASSRLS &
  // REPLICATION anyway.
#if PG16_GTE
  run_prev_utility_hook(args);
#else
  if (is_current_role_privileged(policy->privileged_role)) {
    // Allow `privileged_role` (in addition to superusers) to
    // set bypassrls & replication attributes.
    RUN_ELEVATED(policy->superuser, run_prev_utility_hook(args));
  } else {
    run_prev_utility_hook(args);
  }
#endif

  return true;
}

/*
 * DROP ROLE
 */
static void check_drop_role(DropRoleStmt *stmt, const role_policy *policy) {
  ListCell *item;

  if (!IsTransactionState() || superuser()) {
    return;
  }

  foreach (item, stmt->roles) {
    RoleSpec *role = lfirst_node(RoleSpec, item);

    /*
     * We check only for a named role being dropped; we ignore
     * the special values like PUBLIC, CURRENT_USER, and
     * SESSION_USER. We let Postgres throw its usual error messages
     * for those special values.
     */
    if (role->roletype != ROLESPEC_CSTRING) break;

    if (is_reserved_role(role->rolename, false, policy->reserved_roles))
      EREPORT_RESERVED_ROLE(role->rolename);
  }
}

/*
 * GRANT <role> and REVOKE <role>
 */
static void check_grant_role(GrantRoleStmt *stmt, const role_policy *policy) {
  ListCell *grantee_role_cell;
  ListCell *role_cell;
  bool      role_is_privileged = false;

  if (!IsTransactionState() || superuser()) {
    return;
  }

  /* GRANT <reserved_role> TO <role> */
  if (stmt->is_grant) {
    foreach (role_cell, stmt->granted_roles) {
      AccessPriv *priv = lfirst_node(AccessPriv, role_cell);
      confirm_reserved_memberships(priv->priv_name,
                                   policy->reserved_memberships);
    }
  }

  role_is_privileged = is_current_role_privileged(policy->privileged_role);

  /*
   * GRANT <role> TO <reserved_roles>
   * REVOKE <role> FROM <reserved_roles>
   */
  foreach (grantee_role_cell, stmt->grantee_roles) {
    RoleSpec *spec      = lfirst_node(RoleSpec, grantee_role_cell);
    char     *role_name = get_rolespec_name(spec);
    // privileged_role can do GRANT <role> to <reserved_role>
    if (is_reserved_role(role_name, role_is_privileged, policy->reserved_roles))
      EREPORT_RESERVED_ROLE(role_name);
  }
}

/*
 * ALTER ROLE <role> RENAME TO
 */
static void check_rename_role(RenameStmt *stmt, const role_policy *policy) {
  if (!IsTransactionState() || superuser()) {
    return;
  }

  /* Make sure we only catch "ALTER ROLE <role> RENAME TO" */
  if (stmt->renameType != OBJECT_ROLE) return;

  if (is_reserved_role(stmt->subname, false, policy->reserved_roles))
    EREPORT_RESERVED_ROLE(stmt->subname);

  if (is_reserved_role(stmt->newname, false, policy->reserved_roles))
    EREPORT_RESERVED_ROLE(stmt->newname);
}

bool handle_role_stmt(Node *stmt, const utility_hook_args *args,
                      const role_policy *policy) {
  switch (nodeTag(stmt)) {
  case T_AlterRoleStmt: return alter_role((AlterRoleStmt *)stmt, args, policy);
  case T_AlterRoleSetStmt:
    return alter_role_set((AlterRoleSetStmt *)stmt, args, policy);
  case T_CreateRoleStmt:
    return create_role((CreateRoleStmt *)stmt, args, policy);
  case T_DropRoleStmt:
    check_drop_role((DropRoleStmt *)stmt, policy);
    return false;
  case T_GrantRoleStmt:
    check_grant_role((GrantRoleStmt *)stmt, policy);
    return false;
  case T_RenameStmt:
    check_rename_role((RenameStmt *)stmt, policy);
    return false;
  default: return false;
  }
}
