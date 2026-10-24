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

  // No single named role (ALTER ROLE ALL, PUBLIC): nothing to protect.
  if (target == NULL) return false;

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

  if (target == NULL) return;

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
 * Handlers never see a parse node. Each statement is first reduced to the
 * plain values the policy needs by one of the normalize_* functions below,
 * which are the only place raw node fields are read. A field the grammar can
 * leave absent is documented as nullable on its struct, so the handler has to
 * decide what absence means.
 */

/*
 * ALTER ROLE <role> NOLOGIN NOINHERIT ...
 */
typedef struct {
  const char *role_name;    // NULL for PUBLIC, which is not a role
  bool sets_superuser_attr; // a SUPERUSER / NOSUPERUSER option is present
} alter_role_cmd;

static alter_role_cmd normalize_alter_role(const AlterRoleStmt *stmt) {
  alter_role_cmd cmd = {.role_name = rolespec_name_or_null(stmt->role)};
  ListCell      *lc;

  foreach (lc, stmt->options) {
    DefElem *defel = lfirst_node(DefElem, lc);
    if (strcmp(defel->defname, "superuser") == 0)
      cmd.sets_superuser_attr = true;
  }

  return cmd;
}

static bool alter_role(const alter_role_cmd *cmd, const utility_hook_args *args,
                       const role_policy *policy) {
  // Not a role the policy can protect; core raises its own error.
  if (cmd->role_name == NULL) return false;

  if (is_reserved_role(cmd->role_name, false, policy->reserved_roles))
    EREPORT_RESERVED_ROLE(cmd->role_name);

  if (!is_current_role_privileged(policy->privileged_role)) {
    return false;
  }

  if (is_role_privileged(cmd->role_name, policy->privileged_role)) {
    ereport(ERROR, (errcode(ERRCODE_INSUFFICIENT_PRIVILEGE),
                    errmsg("permission denied to alter role"),
                    errdetail("Only superusers can alter privileged roles.")));
  }

  // Setting the superuser attribute is not allowed.
  if (cmd->sets_superuser_attr) {
    ereport(ERROR, (errcode(ERRCODE_INSUFFICIENT_PRIVILEGE),
                    errmsg("permission denied to alter role"),
                    errdetail("Only roles with the %s attribute may alter "
                              "roles with the %s attribute.",
                              "SUPERUSER", "SUPERUSER")));
  }

  // Allow setting bypassrls & replication.
  RUN_ELEVATED(policy->superuser, run_prev_utility_hook(args));

  return true;
}

/*
 * ALTER ROLE <role> SET search_path TO ...
 */
typedef struct {
  const char *role_name;   // NULL for ALTER ROLE ALL and for PUBLIC
  const char *config_name; // NULL for RESET ALL
} alter_role_set_cmd;

static alter_role_set_cmd
normalize_alter_role_set(const AlterRoleSetStmt *stmt) {
  return (alter_role_set_cmd){
    .role_name   = rolespec_name_or_null(stmt->role),
    .config_name = stmt->setstmt->name,
  };
}

static bool alter_role_set(const alter_role_set_cmd *cmd,
                           const utility_hook_args  *args,
                           const role_policy        *policy) {
  bool role_is_privileged = false;

  // ALTER ROLE ALL targets every role and PUBLIC is not a role: there is no
  // reserved role to protect, and core applies its own stricter permission
  // check for both.
  if (cmd->role_name == NULL) return false;

  role_is_privileged = is_current_role_privileged(policy->privileged_role);

  if (is_reserved_role(cmd->role_name, role_is_privileged,
                       policy->reserved_roles))
    EREPORT_RESERVED_ROLE(cmd->role_name);

  if (!role_is_privileged) {
    return false;
  }

  if (policy->privileged_role_allowed_configs == NULL) {
    return false;
  }
  if (!is_string_in_comma_delimited_string(
          cmd->config_name, policy->privileged_role_allowed_configs)) {
    return false;
  }

  RUN_ELEVATED(policy->superuser, run_prev_utility_hook(args));

  return true;
}

/*
 * CREATE ROLE
 */
typedef struct {
  const char *role_name; // never NULL, the grammar requires a name
  List       *member_of; // char *: IN ROLE/IN GROUP; NULL entries for PUBLIC
  bool        has_role_members; // ROLE / ADMIN / USER <roles> given
  bool        wants_superuser;  // SUPERUSER attribute requested
} create_role_cmd;

static create_role_cmd normalize_create_role(const CreateRoleStmt *stmt) {
  create_role_cmd cmd = {.role_name = stmt->role};
  ListCell       *lc;

  foreach (lc, stmt->options) {
    DefElem *defel = lfirst_node(DefElem, lc);

    if (strcmp(defel->defname, "addroleto") == 0) {
      ListCell *role_cell;
      foreach (role_cell, (List *)defel->arg) {
        RoleSpec *spec = lfirst_node(RoleSpec, role_cell);
        cmd.member_of =
            lappend(cmd.member_of, (void *)rolespec_name_or_null(spec));
      }
    } else if (strcmp(defel->defname, "rolemembers") == 0 ||
               strcmp(defel->defname, "adminmembers") == 0) {
      cmd.has_role_members = true;
    } else if (strcmp(defel->defname, "superuser") == 0 &&
               defGetBoolean(defel)) {
      cmd.wants_superuser = true;
    }
  }

  return cmd;
}

static bool create_role(const create_role_cmd   *cmd,
                        const utility_hook_args *args,
                        const role_policy       *policy) {
  ListCell *lc;

  /* if role already exists, bypass the hook to let it fail with the usual
   * error */
  if (role_exists(cmd->role_name)) return false;

  /* CREATE ROLE <reserved_role> */
  if (is_reserved_role(cmd->role_name, false, policy->reserved_roles))
    EREPORT_RESERVED_ROLE(cmd->role_name);

  // Setting the superuser attribute is not allowed.
  if (cmd->wants_superuser) {
    ereport(ERROR, (errcode(ERRCODE_INSUFFICIENT_PRIVILEGE),
                    errmsg("permission denied to create role"),
                    errdetail("Only roles with the %s attribute may "
                              "create roles with the %s attribute.",
                              "SUPERUSER", "SUPERUSER")));
  }

  /* CREATE ROLE <any_role> IN ROLE/GROUP <role_with_reserved_membership> */
  foreach (lc, cmd->member_of) {
    confirm_reserved_memberships((const char *)lfirst(lc),
                                 policy->reserved_memberships);
  }

  /*
   * CREATE ROLE <role_with_reserved_membership> ROLE/ADMIN/USER <any_role>
   *
   * This is a contrived case because the "role_with_reserved_membership"
   * should already exist, but handle it anyway.
   */
  if (cmd->has_role_members)
    confirm_reserved_memberships(cmd->role_name, policy->reserved_memberships);

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
typedef struct {
  List *role_names; // char *: named roles only. PUBLIC, CURRENT_USER and
                    // SESSION_USER are left for core's usual error.
} drop_role_cmd;

static drop_role_cmd normalize_drop_role(const DropRoleStmt *stmt) {
  drop_role_cmd cmd = {0};
  ListCell     *lc;

  foreach (lc, stmt->roles) {
    RoleSpec *spec = lfirst_node(RoleSpec, lc);
    if (spec->roletype == ROLESPEC_CSTRING)
      cmd.role_names = lappend(cmd.role_names, spec->rolename);
  }

  return cmd;
}

static void check_drop_role(const drop_role_cmd *cmd,
                            const role_policy   *policy) {
  ListCell *lc;

  foreach (lc, cmd->role_names) {
    const char *role_name = lfirst(lc);
    if (is_reserved_role(role_name, false, policy->reserved_roles))
      EREPORT_RESERVED_ROLE(role_name);
  }
}

/*
 * GRANT <role> and REVOKE <role>
 */
typedef struct {
  bool  is_grant;
  List *granted_role_names; // char *: NULL entries where the grammar gave no
                            // name (core rejects those)
  List *grantee_names;      // char *: NULL entries for PUBLIC
} grant_role_cmd;

static grant_role_cmd normalize_grant_role(const GrantRoleStmt *stmt) {
  grant_role_cmd cmd = {.is_grant = stmt->is_grant};
  ListCell      *lc;

  foreach (lc, stmt->granted_roles) {
    cmd.granted_role_names =
        lappend(cmd.granted_role_names, lfirst_node(AccessPriv, lc)->priv_name);
  }
  foreach (lc, stmt->grantee_roles) {
    RoleSpec *spec = lfirst_node(RoleSpec, lc);
    cmd.grantee_names =
        lappend(cmd.grantee_names, (void *)rolespec_name_or_null(spec));
  }

  return cmd;
}

static void check_grant_role(const grant_role_cmd *cmd,
                             const role_policy    *policy) {
  ListCell *lc;
  bool      role_is_privileged = false;

  /* GRANT <reserved_role> TO <role> */
  if (cmd->is_grant) {
    foreach (lc, cmd->granted_role_names) {
      confirm_reserved_memberships(lfirst(lc), policy->reserved_memberships);
    }
  }

  role_is_privileged = is_current_role_privileged(policy->privileged_role);

  /*
   * GRANT <role> TO <reserved_roles>
   * REVOKE <role> FROM <reserved_roles>
   */
  foreach (lc, cmd->grantee_names) {
    const char *role_name = lfirst(lc);
    // privileged_role can do GRANT <role> to <reserved_role>
    if (is_reserved_role(role_name, role_is_privileged, policy->reserved_roles))
      EREPORT_RESERVED_ROLE(role_name);
  }
}

/*
 * ALTER ROLE <role> RENAME TO
 */
typedef struct {
  const char *old_name;
  const char *new_name;
} rename_role_cmd;

// Returns false when the RenameStmt is not "ALTER ROLE <role> RENAME TO".
static bool normalize_rename_role(const RenameStmt *stmt,
                                  rename_role_cmd  *cmd) {
  if (stmt->renameType != OBJECT_ROLE) return false;

  *cmd =
      (rename_role_cmd){.old_name = stmt->subname, .new_name = stmt->newname};
  return true;
}

static void check_rename_role(const rename_role_cmd *cmd,
                              const role_policy     *policy) {
  if (is_reserved_role(cmd->old_name, false, policy->reserved_roles))
    EREPORT_RESERVED_ROLE(cmd->old_name);

  if (is_reserved_role(cmd->new_name, false, policy->reserved_roles))
    EREPORT_RESERVED_ROLE(cmd->new_name);
}

bool handle_role_stmt(Node *stmt, const utility_hook_args *args,
                      const role_policy *policy) {
  switch (nodeTag(stmt)) {
  case T_AlterRoleStmt:
  case T_AlterRoleSetStmt:
  case T_CreateRoleStmt:
  case T_DropRoleStmt:
  case T_GrantRoleStmt:
  case T_RenameStmt      : break;
  default                : return false;
  }

  // Normalizing resolves role names through the catalog, so it needs a
  // transaction. The role policy never applies to superusers.
  if (!IsTransactionState() || superuser()) {
    return false;
  }

  switch (nodeTag(stmt)) {
  case T_AlterRoleStmt: {
    alter_role_cmd cmd = normalize_alter_role((AlterRoleStmt *)stmt);
    return alter_role(&cmd, args, policy);
  }
  case T_AlterRoleSetStmt: {
    alter_role_set_cmd cmd = normalize_alter_role_set((AlterRoleSetStmt *)stmt);
    return alter_role_set(&cmd, args, policy);
  }
  case T_CreateRoleStmt: {
    create_role_cmd cmd = normalize_create_role((CreateRoleStmt *)stmt);
    return create_role(&cmd, args, policy);
  }
  case T_DropRoleStmt: {
    drop_role_cmd cmd = normalize_drop_role((DropRoleStmt *)stmt);
    check_drop_role(&cmd, policy);
    return false;
  }
  case T_GrantRoleStmt: {
    grant_role_cmd cmd = normalize_grant_role((GrantRoleStmt *)stmt);
    check_grant_role(&cmd, policy);
    return false;
  }
  case T_RenameStmt: {
    rename_role_cmd cmd;
    if (!normalize_rename_role((RenameStmt *)stmt, &cmd)) return false;
    check_rename_role(&cmd, policy);
    return false;
  }
  default: return false;
  }
}
