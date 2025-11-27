#include "pg_prelude.h"

#include "constrained_extensions.h"
#include "drop_trigger_grants.h"
#include "event_triggers.h"
#include "extension_custom_scripts.h"
#include "extensions_parameter_overrides.h"
#include "policy_grants.h"
#include "privileged_extensions.h"

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

#define EREPORT_INVALID_PARAMETER(name)                                        \
  ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE),                    \
                  errmsg("parameter \"%s\" must be a comma-separated list of " \
                         "identifiers",                                        \
                         name)));

#define MAX_CONSTRAINED_EXTENSIONS 100
#define MAX_EXTENSIONS_PARAMETER_OVERRIDES 100
#define MAX_DROP_TRIGGER_GRANTS 100
#define MAX_POLICY_GRANTS 100

/* required macro for extension libraries to work */
PG_MODULE_MAGIC;

static char *reserved_roles                 = NULL;
static char *reserved_memberships           = NULL;
static char *placeholders                   = NULL;
static char *placeholders_disallowed_values = NULL;
static char *empty_placeholder              = NULL;
static char *privileged_extensions          = NULL;
static char *supautils_superuser            = NULL;
static char *extension_custom_scripts_path  = NULL;
static char *privileged_role = NULL; // the privileged_role is a proxy role for
                                     // the `supautils.superuser` role
static char *privileged_role_allowed_configs = NULL;

static ProcessUtility_hook_type prev_hook            = NULL;
static fmgr_hook_type           next_fmgr_hook       = NULL;
static needs_fmgr_hook_type     next_needs_fmgr_hook = NULL;

static char                 *constrained_extensions_str        = NULL;
static constrained_extension cexts[MAX_CONSTRAINED_EXTENSIONS] = {0};
static size_t                total_cexts                       = 0;

static char                         *extensions_parameter_overrides_str = NULL;
static extension_parameter_overrides epos[MAX_EXTENSIONS_PARAMETER_OVERRIDES] =
    {0};
static size_t total_epos = 0;

static char         *policy_grants_str      = NULL;
static policy_grants pgs[MAX_POLICY_GRANTS] = {0};
static size_t        total_pgs              = 0;

static char               *drop_trigger_grants_str       = NULL;
static drop_trigger_grants dtgs[MAX_DROP_TRIGGER_GRANTS] = {0};
static size_t              total_dtgs                    = 0;

bool        log_skipped_evtrigs  = false;
static bool disable_copy_program = false;

void _PG_init(void);
void _PG_fini(void);

static bool is_reserved_role(const char *target, bool allow_configurable_roles);

static void confirm_reserved_memberships(const char *target);

static void check_parameter(char *val, char *name);

static bool is_current_role_privileged(void);

static bool is_role_privileged(const char *role);

// the hook will only be attached to functions that `RETURN event_trigger`
static bool supautils_needs_fmgr_hook(Oid functionId) {
  if (next_needs_fmgr_hook && (*next_needs_fmgr_hook)(functionId)) return true;

  return is_event_trigger_function(functionId);
}

static void skip_event_trigger(FmgrInfo *flinfo, const char *func_name,
                               const char *current_role_name,
                               const char *role_descriptor,
                               const char *function_condition,
                               const char *owner_name) {
  if (log_skipped_evtrigs) {
    ereport(NOTICE,
            errmsg("Skipping event trigger function \"%s\" for user \"%s\"",
                   func_name, current_role_name),
            errdetail("\"%s\" is %s and the function \"%s\" %s \"%s\"",
                      current_role_name, role_descriptor, func_name,
                      function_condition, owner_name));
  }

  // we can't skip execution directly inside the fmgr_hook (although we can
  // abort it with ereport) so instead we use the workaround of changing the
  // function to a noop function
  force_noop(flinfo);
}

// This function will fire twice: once before execution of the database function
// (event=FHET_START) and once after execution has finished or failed
// (event=FHET_END/FHET_ABORT).
static void supautils_fmgr_hook(FmgrHookEventType event, FmgrInfo *flinfo,
                                Datum *private) {
  switch (event) {
  // we only need to change behavior before the function gets executed
  case FHET_START: {
    if (is_event_trigger_function(
            flinfo->fn_oid)) { // recheck the function is an event trigger in
                               // case another extension need_fmgr_hook passed
                               // our supautils_needs_fmgr_hook
      func_attrs fattrs = get_function_attrs(
          (func_search){.as = FO_SEARCH_FINFO, .val.finfo = flinfo});
      const Oid current_role_oid =
          fattrs.is_security_definer
              ?
              // when the function is security definer, we need to get the
              // session user id otherwise it will fire for superusers or
              // reserved roles. See
              // https://github.com/supabase/supautils/issues/140.
              GetOuterUserId()
              : GetUserId();
      const char *current_role_name =
          GetUserNameFromId(current_role_oid, false);
      const bool  role_is_super    = superuser_arg(current_role_oid);
      const bool  role_is_reserved = is_reserved_role(current_role_name, false);
      const bool  function_is_owned_by_super = superuser_arg(fattrs.owner);
      const bool  role_is_function_owner     = current_role_oid == fattrs.owner;
      const char *func_name                  = get_func_name(flinfo->fn_oid);
      const char *function_owner_name = GetUserNameFromId(fattrs.owner, false);
      if (role_is_super) {
        if (!function_is_owned_by_super) {
          skip_event_trigger(
              flinfo, func_name, current_role_name, "a superuser",
              "is not superuser-owned, it's owned by", function_owner_name);
        } else if (!role_is_function_owner) {
          skip_event_trigger(flinfo, func_name, current_role_name,
                             "a superuser",
                             "is not owned by the same role, it's owned by",
                             function_owner_name);
        }
      } else if (role_is_reserved) {
        if (!function_is_owned_by_super) {
          skip_event_trigger(
              flinfo, func_name, current_role_name, "a reserved role",
              "is not superuser-owned, it's owned by", function_owner_name);
        }
      }
    }

    if (next_fmgr_hook) (*next_fmgr_hook)(event, flinfo, private);
    break;
  }

  // do nothing when the function already executed
  case FHET_END:
  case FHET_ABORT:
    if (next_fmgr_hook) (*next_fmgr_hook)(event, flinfo, private);
    break;

  default: elog(ERROR, "unexpected event type: %d", (int)event); break;
  }
}

static void supautils_hook(PROCESS_UTILITY_PARAMS) {
  /* Get the utility statement from the planned statement */
  Node *utility_stmt = pstmt->utilityStmt;

  switch (utility_stmt->type) {
  /*
   * ALTER ROLE <role> NOLOGIN NOINHERIT..
   */
  case T_AlterRoleStmt: {
    AlterRoleStmt *stmt        = (AlterRoleStmt *)utility_stmt;
    ListCell      *option_cell = NULL;
    bool           already_switched_to_superuser = false;

    if (!IsTransactionState()) {
      break;
    }
    if (superuser()) {
      break;
    }

    char *role_name = get_rolespec_name(stmt->role);

    if (is_reserved_role(role_name, false)) EREPORT_RESERVED_ROLE(role_name);

    if (!is_current_role_privileged()) {
      break;
    }

    if (is_role_privileged(stmt->role->rolename)) {
      ereport(ERROR,
              (errcode(ERRCODE_INSUFFICIENT_PRIVILEGE),
               errmsg("permission denied to alter role"),
               errdetail("Only superusers can alter privileged roles.")));
    }

    // Setting the superuser attribute is not allowed.
    foreach (option_cell, stmt->options) {
      DefElem *defel = (DefElem *)lfirst(option_cell);
      if (strcmp(defel->defname, "superuser") == 0) {
        ereport(ERROR, (errcode(ERRCODE_INSUFFICIENT_PRIVILEGE),
                        errmsg("permission denied to alter role"),
                        errdetail("Only roles with the %s attribute may alter "
                                  "roles with the %s attribute.",
                                  "SUPERUSER", "SUPERUSER")));
      }
    }

    // Allow setting bypassrls & replication.
    switch_to_superuser(supautils_superuser, &already_switched_to_superuser);

    run_process_utility_hook_with_cleanup(
        prev_hook, already_switched_to_superuser, switch_to_original_role);

    if (!already_switched_to_superuser) {
      switch_to_original_role();
    }

    return;
  }

  /*
   * ALTER ROLE <role> SET search_path TO ...
   */
  case T_AlterRoleSetStmt: {
    AlterRoleSetStmt *stmt               = (AlterRoleSetStmt *)utility_stmt;
    bool              role_is_privileged = false;

    if (!IsTransactionState()) {
      break;
    }
    if (superuser()) {
      break;
    }

    role_is_privileged = is_current_role_privileged();

    char *role_name = get_rolespec_name(stmt->role);

    if (is_reserved_role(role_name, role_is_privileged))
      EREPORT_RESERVED_ROLE(role_name);

    if (!role_is_privileged) {
      break;
    }

    if (privileged_role_allowed_configs == NULL) {
      break;
    } else {
      bool is_privileged_role_allowed_config =
          is_string_in_comma_delimited_string(
              ((VariableSetStmt *)stmt->setstmt)->name,
              privileged_role_allowed_configs);

      if (!is_privileged_role_allowed_config) {
        break;
      }
    }

    {
      bool already_switched_to_superuser = false;
      switch_to_superuser(supautils_superuser, &already_switched_to_superuser);

      run_process_utility_hook_with_cleanup(
          prev_hook, already_switched_to_superuser, switch_to_original_role);

      if (!already_switched_to_superuser) {
        switch_to_original_role();
      }

      return;
    }

    return;
  }

  /*
   * CREATE ROLE
   */
  case T_CreateRoleStmt: {
    if (IsTransactionState() && !superuser()) {
      CreateRoleStmt *stmt         = (CreateRoleStmt *)utility_stmt;
      const char     *created_role = stmt->role;
      List           *addroleto    = NIL; /* roles to make this a member of */
      bool hasrolemembers = false; /* has roles to be members of this role */
      ListCell *option_cell;

      /* if role already exists, bypass the hook to let it fail with the usual
       * error */
      if (OidIsValid(get_role_oid(created_role, true))) break;

      /* CREATE ROLE <reserved_role> */
      if (is_reserved_role(created_role, false))
        EREPORT_RESERVED_ROLE(created_role);

      /* Check to see if there are any descriptions related to membership. */
      foreach (option_cell, stmt->options) {
        DefElem *defel = (DefElem *)lfirst(option_cell);
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
          RoleSpec *rolemember = lfirst(role_cell);
          confirm_reserved_memberships(get_rolespec_name(rolemember));
        }
      }

      /*
       * CREATE ROLE <role_with_reserved_membership> ROLE/ADMIN/USER <any_role>
       *
       * This is a contrived case because the "role_with_reserved_membership"
       * should already exist, but handle it anyway.
       */
      if (hasrolemembers) confirm_reserved_memberships(created_role);

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
      run_process_utility_hook(prev_hook);
#else
      if (is_current_role_privileged()) {
        bool already_switched_to_superuser = false;

        // Allow `privileged_role` (in addition to superusers) to
        // set bypassrls & replication attributes.
        switch_to_superuser(supautils_superuser,
                            &already_switched_to_superuser);

        run_process_utility_hook_with_cleanup(
            prev_hook, already_switched_to_superuser, switch_to_original_role);

        if (!already_switched_to_superuser) {
          switch_to_original_role();
        }
      } else {
        run_process_utility_hook(prev_hook);
      }
#endif

      return;
    }
    break;
  }

  /*
   * DROP ROLE
   */
  case T_DropRoleStmt: {
    if (IsTransactionState() && !superuser()) {
      DropRoleStmt *stmt = (DropRoleStmt *)utility_stmt;
      ListCell     *item;

      foreach (item, stmt->roles) {
        RoleSpec *role = lfirst(item);

        /*
         * We check only for a named role being dropped; we ignore
         * the special values like PUBLIC, CURRENT_USER, and
         * SESSION_USER. We let Postgres throw its usual error messages
         * for those special values.
         */
        if (role->roletype != ROLESPEC_CSTRING) break;

        if (is_reserved_role(role->rolename, false))
          EREPORT_RESERVED_ROLE(role->rolename);
      }
    }
    break;
  }

  /*
   * GRANT <role> and REVOKE <role>
   */
  case T_GrantRoleStmt: {
    if (IsTransactionState() && !superuser()) {
      GrantRoleStmt *stmt = (GrantRoleStmt *)utility_stmt;
      ListCell      *grantee_role_cell;
      ListCell      *role_cell;
      bool           role_is_privileged = false;

      /* GRANT <reserved_role> TO <role> */
      if (stmt->is_grant) {
        foreach (role_cell, stmt->granted_roles) {
          AccessPriv *priv = (AccessPriv *)lfirst(role_cell);
          confirm_reserved_memberships(priv->priv_name);
        }
      }

      role_is_privileged = is_current_role_privileged();

      /*
       * GRANT <role> TO <reserved_roles>
       * REVOKE <role> FROM <reserved_roles>
       */
      foreach (grantee_role_cell, stmt->grantee_roles) {
        AccessPriv *priv = (AccessPriv *)lfirst(grantee_role_cell);
        // privileged_role can do GRANT <role> to <reserved_role>
        if (is_reserved_role(priv->priv_name, role_is_privileged))
          EREPORT_RESERVED_ROLE(priv->priv_name);
      }
    }
    break;
  }

  /*
   * All RENAME statements are caught here
   */
  case T_RenameStmt: {
    if (IsTransactionState() && !superuser()) {
      RenameStmt *stmt = (RenameStmt *)utility_stmt;

      /* Make sure we only catch "ALTER ROLE <role> RENAME TO" */
      if (stmt->renameType != OBJECT_ROLE) break;

      if (is_reserved_role(stmt->subname, false))
        EREPORT_RESERVED_ROLE(stmt->subname);

      if (is_reserved_role(stmt->newname, false))
        EREPORT_RESERVED_ROLE(stmt->newname);
    }
    break;
  }

  /*
   * CREATE EXTENSION <extension>
   */
  case T_CreateExtensionStmt: {
    CreateExtensionStmt *stmt = (CreateExtensionStmt *)utility_stmt;

    constrain_extension(stmt->extname, cexts, total_cexts);

    bool already_switched_to_superuser = false;

    switch_to_superuser(supautils_superuser, &already_switched_to_superuser);

    run_global_before_create_script(stmt->extname, stmt->options,
                                    extension_custom_scripts_path);

    run_ext_before_create_script(stmt->extname, stmt->options,
                                 extension_custom_scripts_path);

    override_create_ext_statement(stmt, total_epos, epos);

    if (is_extension_privileged(stmt->extname, privileged_extensions)) {
      run_process_utility_hook_with_cleanup(
          prev_hook, already_switched_to_superuser, switch_to_original_role);
    } else {
      if (!already_switched_to_superuser) {
        switch_to_original_role();
      }

      run_process_utility_hook(prev_hook);

      switch_to_superuser(supautils_superuser, &already_switched_to_superuser);
    }

    run_ext_after_create_script(stmt->extname, stmt->options,
                                extension_custom_scripts_path);

    if (!already_switched_to_superuser) {
      switch_to_original_role();
    }

    return;
  }

  /*
   * ALTER EXTENSION <extension> [ ADD | DROP | UPDATE ]
   */
  case T_AlterExtensionStmt: {
    if (superuser()) {
      break;
    }

    AlterExtensionStmt *stmt = (AlterExtensionStmt *)pstmt->utilityStmt;

    if (is_extension_privileged(stmt->extname, privileged_extensions)) {
      bool already_switched_to_superuser = false;

      switch_to_superuser(supautils_superuser, &already_switched_to_superuser);

      run_process_utility_hook_with_cleanup(
          prev_hook, already_switched_to_superuser, switch_to_original_role);

      if (!already_switched_to_superuser) {
        switch_to_original_role();
      }
    }

    break;
  }

  /*
   * ALTER EXTENSION <extension> SET SCHEMA
   */
  case T_AlterObjectSchemaStmt: {
    if (superuser()) {
      break;
    }

    AlterObjectSchemaStmt *stmt = (AlterObjectSchemaStmt *)pstmt->utilityStmt;

    if (stmt->objectType == OBJECT_EXTENSION &&
        is_extension_privileged(strVal(stmt->object), privileged_extensions)) {
      bool already_switched_to_superuser = false;

      switch_to_superuser(supautils_superuser, &already_switched_to_superuser);

      run_process_utility_hook_with_cleanup(
          prev_hook, already_switched_to_superuser, switch_to_original_role);

      if (!already_switched_to_superuser) {
        switch_to_original_role();
      }

      return;
    }

    break;
  }

  /*
   * ALTER EXTENSION <extension> [ ADD | DROP ]
   *
   * Not supported. Fall back to normal behavior.
   */
  case T_AlterExtensionContentsStmt: break;

  /**
   * CREATE FOREIGN DATA WRAPPER <fdw>
   */
  case T_CreateFdwStmt: {
    const Oid current_user_id               = GetUserId();
    bool      already_switched_to_superuser = false;

    if (superuser()) {
      break;
    }
    if (!is_current_role_privileged()) {
      break;
    }

    switch_to_superuser(supautils_superuser, &already_switched_to_superuser);

    run_process_utility_hook_with_cleanup(
        prev_hook, already_switched_to_superuser, switch_to_original_role);

    CreateFdwStmt *stmt = (CreateFdwStmt *)utility_stmt;

    // Change FDW owner to the current role (which is a privileged role)
    alter_owner(stmt->fdwname, current_user_id, ALT_FDW);

    if (!already_switched_to_superuser) {
      switch_to_original_role();
    }

    return;
  }

  /**
   * CREATE PUBLICATION
   */
  case T_CreatePublicationStmt: {
    const Oid current_user_id               = GetUserId();
    bool      already_switched_to_superuser = false;

    if (superuser()) {
      break;
    }
    if (!is_current_role_privileged()) {
      break;
    }

    switch_to_superuser(supautils_superuser, &already_switched_to_superuser);

    run_process_utility_hook_with_cleanup(
        prev_hook, already_switched_to_superuser, switch_to_original_role);

    CreatePublicationStmt *stmt = (CreatePublicationStmt *)utility_stmt;

    // Change publication owner to the current role (which is a privileged role)
    alter_owner(stmt->pubname, current_user_id, ALT_PUB);

    if (!already_switched_to_superuser) {
      switch_to_original_role();
    }

    return;
  }

  /**
   * ALTER PUBLICATION <name> ADD TABLES IN SCHEMA ...
   */
  case T_AlterPublicationStmt: {
    bool already_switched_to_superuser = false;

    if (superuser()) {
      break;
    }
    if (!is_current_role_privileged()) {
      break;
    }

    switch_to_superuser(supautils_superuser, &already_switched_to_superuser);

    run_process_utility_hook_with_cleanup(
        prev_hook, already_switched_to_superuser, switch_to_original_role);

    if (!already_switched_to_superuser) {
      switch_to_original_role();
    }

    return;
  }

  /**
   * CREATE POLICY
   */
  case T_CreatePolicyStmt: {
    CreatePolicyStmt *stmt = (CreatePolicyStmt *)utility_stmt;

    if (superuser()) {
      break;
    }

    if (is_current_role_granted_table_policy(stmt->table, pgs, total_pgs)) {
      bool already_switched_to_superuser = false;

      switch_to_superuser(supautils_superuser, &already_switched_to_superuser);

      run_process_utility_hook_with_cleanup(
          prev_hook, already_switched_to_superuser, switch_to_original_role);

      if (!already_switched_to_superuser) {
        switch_to_original_role();
      }

      return;
    }

    break;
  }

  /**
   * ALTER POLICY
   */
  case T_AlterPolicyStmt: {
    AlterPolicyStmt *stmt = (AlterPolicyStmt *)utility_stmt;

    if (superuser()) {
      break;
    }

    if (is_current_role_granted_table_policy(stmt->table, pgs, total_pgs)) {
      bool already_switched_to_superuser = false;

      switch_to_superuser(supautils_superuser, &already_switched_to_superuser);

      run_process_utility_hook_with_cleanup(
          prev_hook, already_switched_to_superuser, switch_to_original_role);

      if (!already_switched_to_superuser) {
        switch_to_original_role();
      }

      return;
    }

    break;
  }

  case T_DropStmt: {
    DropStmt *stmt = (DropStmt *)utility_stmt;

    if (superuser()) {
      break;
    }

    switch (stmt->removeType) {
    /*
     * DROP EXTENSION <extension>
     */
    case OBJECT_EXTENSION: {
      if (all_extensions_are_privileged(stmt->objects, privileged_extensions)) {
        bool already_switched_to_superuser = false;
        switch_to_superuser(supautils_superuser,
                            &already_switched_to_superuser);

        run_process_utility_hook_with_cleanup(
            prev_hook, already_switched_to_superuser, switch_to_original_role);

        if (!already_switched_to_superuser) {
          switch_to_original_role();
        }

        return;
      }

      break;
    }

    /*
     * DROP POLICY
     */
    case OBJECT_POLICY: {
      // DROP POLICY always has one object.
      ListCell *object_cell = list_head(stmt->objects);
      List     *object      = castNode(List, lfirst(object_cell));
      // Last element is the policy name, the rest is the table name.
      // Take everything but the last.
      List *table_name_list =
          list_truncate(list_copy(object), list_length(object) - 1);
      RangeVar *table_range_var = makeRangeVarFromNameList(table_name_list);
      bool      already_switched_to_superuser = false;

      if (!is_current_role_granted_table_policy(table_range_var, pgs,
                                                total_pgs)) {
        break;
      }

      switch_to_superuser(supautils_superuser, &already_switched_to_superuser);

      run_process_utility_hook_with_cleanup(
          prev_hook, already_switched_to_superuser, switch_to_original_role);

      if (!already_switched_to_superuser) {
        switch_to_original_role();
      }

      return;
    }

    /*
     * DROP TRIGGER
     */
    case OBJECT_TRIGGER: {
      // DROP TRIGGER always has one object.
      ListCell *object_cell = list_head(stmt->objects);
      List     *object      = castNode(List, lfirst(object_cell));
      // Last element is the trigger name, the rest is the table name.
      // Take everything but the last.
      List *table_name_list =
          list_truncate(list_copy(object), list_length(object) - 1);
      RangeVar *table_range_var = makeRangeVarFromNameList(table_name_list);
      bool      already_switched_to_superuser = false;

      if (!is_current_role_granted_table_drop_trigger(table_range_var, dtgs,
                                                      total_dtgs)) {
        break;
      }

      switch_to_superuser(supautils_superuser, &already_switched_to_superuser);

      run_process_utility_hook_with_cleanup(
          prev_hook, already_switched_to_superuser, switch_to_original_role);

      if (!already_switched_to_superuser) {
        switch_to_original_role();
      }

      return;
    }

    default: break;
    }

    break;
  }

  case T_CommentStmt: {
    if (!IsTransactionState()) {
      break;
    }
    if (superuser()) {
      break;
    }
    if (((CommentStmt *)utility_stmt)->objtype != OBJECT_EXTENSION) {
      break;
    }
    if (!is_current_role_privileged()) {
      break;
    }

    {
      bool already_switched_to_superuser = false;
      switch_to_superuser(supautils_superuser, &already_switched_to_superuser);

      run_process_utility_hook_with_cleanup(
          prev_hook, already_switched_to_superuser, switch_to_original_role);

      if (!already_switched_to_superuser) {
        switch_to_original_role();
      }

      return;
    }
  }

  case T_VariableSetStmt: {
    if (!IsTransactionState()) {
      break;
    }
    if (superuser()) {
      break;
    }
    if (privileged_role_allowed_configs == NULL) {
      break;
    } else {
      bool is_privileged_role_allowed_config =
          is_string_in_comma_delimited_string(
              ((VariableSetStmt *)utility_stmt)->name,
              privileged_role_allowed_configs);

      if (!is_privileged_role_allowed_config) {
        break;
      }
    }
    if (!is_current_role_privileged()) {
      break;
    }

    {
      bool already_switched_to_superuser = false;
      switch_to_superuser(supautils_superuser, &already_switched_to_superuser);

      run_process_utility_hook_with_cleanup(
          prev_hook, already_switched_to_superuser, switch_to_original_role);

      if (!already_switched_to_superuser) {
        switch_to_original_role();
      }

      return;
    }
  }

  case T_CreateEventTrigStmt: {
    if (!IsTransactionState()) {
      break;
    }

    if (!is_current_role_privileged()) {
      break;
    }

    {
      bool      already_switched_to_superuser = false;
      const Oid current_user_id               = GetUserId();

      CreateEventTrigStmt *stmt = (CreateEventTrigStmt *)utility_stmt;

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

      switch_to_superuser(supautils_superuser, &already_switched_to_superuser);

      run_process_utility_hook_with_cleanup(
          prev_hook, already_switched_to_superuser, switch_to_original_role);

      if (!current_user_is_super)
        // Change event trigger owner to the current role (which is a privileged
        // role)
        alter_owner(stmt->trigname, current_user_id, ALT_EVTRIG);

      if (!already_switched_to_superuser) {
        switch_to_original_role();
      }

      return;
    }
  }

  case T_CopyStmt: {
    CopyStmt *stmt = (CopyStmt *)utility_stmt;

    if (stmt->is_program && disable_copy_program) {
      ereport(ERROR,
              (errmsg("COPY TO/FROM PROGRAM not allowed"),
               errdetail(
                   "The copy to/from program utility statement is disabled")));
    }

    break;
  }

  default: break;
  }

  /* Chain to previously defined hooks */
  run_process_utility_hook(prev_hook);
}

static bool extensions_parameter_overrides_check_hook(
    char **newval, __attribute__((unused)) void **extra,
    __attribute__((unused)) GucSource source) {
  char *val = *newval;

  if (total_epos > 0) {
    for (size_t i = 0; i < total_epos; i++) {
      pfree(epos[i].name);
      pfree(epos[i].schema);
    }
    total_epos = 0;
  }

  if (val) {
    json_extension_parameter_overrides_parse_state state =
        parse_extensions_parameter_overrides(val, epos);
    if (state.error_msg) {
      ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
                      errmsg("supautils.extensions_parameter_overrides: %s",
                             state.error_msg)));
    }
    total_epos = state.total_epos;
  }

  return true;
}

static bool policy_grants_check_hook(char                            **newval,
                                     __attribute__((unused)) void    **extra,
                                     __attribute__((unused)) GucSource source) {
  char *val = *newval;

  for (size_t i = 0; i < total_pgs; i++) {
    pfree(pgs[i].role_name);
    for (size_t j = 0; j < pgs[i].total_tables; j++) {
      pfree(pgs[i].table_names[j]);
    }
    pgs[i].total_tables = 0;
  }
  total_pgs = 0;

  if (val) {
    json_policy_grants_parse_state state = parse_policy_grants(val, pgs);
    if (state.error_msg) {
      ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
                      errmsg("supautils.policy_grants: %s", state.error_msg)));
    }
    total_pgs = state.total_pgs;
  }

  return true;
}

static bool drop_trigger_grants_check_hook(char **newval,
                                           __attribute__((unused)) void **extra,
                                           __attribute__((unused))
                                           GucSource source) {
  char *val = *newval;

  for (size_t i = 0; i < total_dtgs; i++) {
    pfree(dtgs[i].role_name);
    for (size_t j = 0; j < dtgs[i].total_tables; j++) {
      pfree(dtgs[i].table_names[j]);
    }
    dtgs[i].total_tables = 0;
  }
  total_dtgs = 0;

  if (val) {
    json_drop_trigger_grants_parse_state state =
        parse_drop_trigger_grants(val, dtgs);
    if (state.error_msg) {
      ereport(ERROR,
              (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
               errmsg("supautils.drop_trigger_grants: %s", state.error_msg)));
    }
    total_dtgs = state.total_dtgs;
  }

  return true;
}

static bool reserved_roles_check_hook(char                         **newval,
                                      __attribute__((unused)) void **extra,
                                      __attribute__((unused))
                                      GucSource source) {
  check_parameter(*newval, "supautils.reserved_roles");

  return true;
}

static bool
reserved_memberships_check_hook(char                            **newval,
                                __attribute__((unused)) void    **extra,
                                __attribute__((unused)) GucSource source) {
  check_parameter(*newval, "supautils.reserved_memberships");

  return true;
}

static bool placeholders_disallowed_values_check_hook(
    char **newval, __attribute__((unused)) void **extra,
    __attribute__((unused)) GucSource source) {
  check_parameter(*newval, "supautils.placeholders_disallowed_values");

  return true;
}

static bool
privileged_extensions_check_hook(char                            **newval,
                                 __attribute__((unused)) void    **extra,
                                 __attribute__((unused)) GucSource source) {
  check_parameter(*newval, "supautils.privileged_extensions");

  return true;
}

static bool privileged_role_allowed_configs_check_hook(
    char **newval, __attribute__((unused)) void **extra,
    __attribute__((unused)) GucSource source) {
  check_parameter(*newval, "supautils.privileged_role_allowed_configs");

  return true;
}

static bool disable_program_guc_check_hook(__attribute__((unused)) bool *newval,
                                           __attribute__((unused)) void **extra,
                                           GucSource source) {
  // only allow setting from the postgresql.conf or the default value
  // ALTER SYSTEM changes the PGC_S_GLOBAL, so this also prevents
  // postgresql.auto.conf based changes
  return source == PGC_S_FILE || source == PGC_S_DEFAULT;
}

static void check_parameter(char *val, char *name) {
  List *comma_separated_list;

  if (val != NULL) {
    if (!SplitIdentifierString(pstrdup(val), ',', &comma_separated_list))
      EREPORT_INVALID_PARAMETER(name);

    list_free(comma_separated_list);
  }
}

static void
constrained_extensions_assign_hook(const char                   *newval,
                                   __attribute__((unused)) void *extra) {
  if (total_cexts > 0) {
    for (size_t i = 0; i < total_cexts; i++) {
      pfree(cexts[i].name);
    }
    total_cexts = 0;
  }
  if (newval) {
    json_constrained_extension_parse_state state =
        parse_constrained_extensions(newval, cexts);
    total_cexts = state.total_cexts;
    if (state.error_msg) {
      ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
                      errmsg("supautils.constrained_extensions: %s",
                             state.error_msg)));
    }
  }
}

static bool is_reserved_role(const char *target,
                             bool        allow_configurable_roles) {
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

static void confirm_reserved_memberships(const char *target) {
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

static bool placeholders_check_hook(char                            **newval,
                                    __attribute__((unused)) void    **extra,
                                    __attribute__((unused)) GucSource source) {
  char *val = *newval;

  if (val) {
    List     *comma_separated_list;
    ListCell *cell;
    bool      saw_sep = false;

    if (!SplitIdentifierString(pstrdup(val), ',', &comma_separated_list))
      EREPORT_INVALID_PARAMETER("supautils.placeholders");

    foreach (cell, comma_separated_list) {
      for (const char *p = lfirst(cell); *p; p++) {
        // check if the GUC has a "." in it(if it's a placeholder)
        if (*p == GUC_QUALIFIER_SEPARATOR) saw_sep = true;
      }
    }

    list_free(comma_separated_list);

    if (!saw_sep)
      ereport(ERROR,
              (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
               errmsg("supautils.placeholders must contain guc placeholders")));
  }

  return true;
}

static bool
restrict_placeholders_check_hook(char                            **newval,
                                 __attribute__((unused)) void    **extra,
                                 __attribute__((unused)) GucSource source) {
  bool not_empty = placeholders_disallowed_values &&
                   placeholders_disallowed_values[0] != '\0';

  if (*newval && not_empty) {
    char *token, *string, *tofree;
    char *val = str_tolower(*newval, strlen(*newval), DEFAULT_COLLATION_OID);

    tofree = string = pstrdup(placeholders_disallowed_values);

    while ((token = strsep(&string, ",")) != NULL) {
      if (strstr(val, token)) {
        GUC_check_errcode(ERRCODE_INVALID_PARAMETER_VALUE);
        GUC_check_errmsg("The placeholder contains the \"%s\" disallowed value",
                         token);
        pfree(tofree);
        pfree(val);
        return false;
      }
    }

    pfree(tofree);
    pfree(val);
  }

  return true;
}

static bool is_current_role_privileged(void) {
  Oid current_role_oid = GetUserId();
  Oid privileged_role_oid;

  if (privileged_role == NULL) {
    return false;
  }
  privileged_role_oid = get_role_oid(privileged_role, true);

  return OidIsValid(privileged_role_oid) &&
         has_privs_of_role(current_role_oid, privileged_role_oid);
}

static bool is_role_privileged(const char *role) {
  Oid role_oid = get_role_oid(role, true);
  Oid privileged_role_oid;

  if (privileged_role == NULL) {
    return false;
  }
  privileged_role_oid = get_role_oid(privileged_role, true);

  return OidIsValid(role_oid) && OidIsValid(privileged_role_oid) &&
         has_privs_of_role(role_oid, privileged_role_oid);
}

void _PG_init(void) {

  // process utility hook
  prev_hook           = ProcessUtility_hook;
  ProcessUtility_hook = supautils_hook;

  // fmgr hook
  next_needs_fmgr_hook = needs_fmgr_hook;
  needs_fmgr_hook      = supautils_needs_fmgr_hook;

  next_fmgr_hook = fmgr_hook;
  fmgr_hook      = supautils_fmgr_hook;

  DefineCustomStringVariable(
      "supautils.extensions_parameter_overrides",
      "Overrides for CREATE EXTENSION parameters", NULL,
      &extensions_parameter_overrides_str, NULL, PGC_SIGHUP, 0,
      &extensions_parameter_overrides_check_hook, NULL, NULL);

  DefineCustomStringVariable(
      "supautils.reserved_roles",
      "Comma-separated list of roles that cannot be modified", NULL,
      &reserved_roles, NULL, PGC_SIGHUP, 0, reserved_roles_check_hook, NULL,
      NULL);

  DefineCustomStringVariable(
      "supautils.reserved_memberships",
      "Comma-separated list of roles whose memberships cannot be granted", NULL,
      &reserved_memberships, NULL, PGC_SIGHUP, 0,
      reserved_memberships_check_hook, NULL, NULL);

  DefineCustomStringVariable(
      "supautils.placeholders",
      "GUC placeholders which will get values disallowed according to "
      "supautils.placeholders_disallowed_values",
      NULL, &placeholders, NULL, PGC_SIGHUP, 0, placeholders_check_hook, NULL,
      NULL);

  DefineCustomStringVariable(
      "supautils.placeholders_disallowed_values",
      "disallowed values for the GUC placeholders defined in "
      "supautils.placeholders",
      NULL, &placeholders_disallowed_values, NULL, PGC_SIGHUP, 0,
      placeholders_disallowed_values_check_hook, NULL, NULL);

  DefineCustomStringVariable("supautils.privileged_extensions",
                             "Comma-separated list of extensions which get "
                             "installed using supautils.superuser",
                             NULL, &privileged_extensions, NULL, PGC_SIGHUP, 0,
                             privileged_extensions_check_hook, NULL, NULL);

  DefineCustomStringVariable(
      "supautils.privileged_extensions_custom_scripts_path",
      "Path to load privileged extensions' custom scripts from. Deprecated: "
      "use supautils.extension_custom_scripts_path instead.",
      NULL, &extension_custom_scripts_path, NULL, PGC_SIGHUP, 0, NULL, NULL,
      NULL);

  DefineCustomStringVariable("supautils.extension_custom_scripts_path",
                             "Path to load extension custom scripts from", NULL,
                             &extension_custom_scripts_path, NULL, PGC_SIGHUP,
                             0, NULL, NULL, NULL);

  DefineCustomStringVariable(
      "supautils.superuser",
      "Superuser to install extensions in supautils.privileged_extensions as",
      NULL, &supautils_superuser, NULL, PGC_SIGHUP, 0, NULL, NULL, NULL);

  // TODO emit a warning when this deprecated GUC is used
  DefineCustomStringVariable(
      "supautils.privileged_extensions_superuser",
      "Superuser to install extensions in supautils.privileged_extensions "
      "as. Deprecated: use supautils.superuser instead.",
      NULL, &supautils_superuser, NULL, PGC_SIGHUP, 0, NULL, NULL, NULL);

  DefineCustomStringVariable(
      "supautils.privileged_role",
      "Non-superuser role to be granted with some superuser privileges", NULL,
      &privileged_role, NULL, PGC_SIGHUP, 0, NULL, NULL, NULL);

  DefineCustomStringVariable(
      "supautils.privileged_role_allowed_configs",
      "Superuser-only configs that the privileged_role is allowed to configure",
      NULL, &privileged_role_allowed_configs, NULL, PGC_SIGHUP, 0,
      privileged_role_allowed_configs_check_hook, NULL, NULL);

  DefineCustomStringVariable("supautils.constrained_extensions",
                             "Extensions that require a minimum amount of "
                             "CPUs, memory and free disk to be installed",
                             NULL, &constrained_extensions_str, NULL,
                             SUPAUTILS_GUC_CONTEXT_SIGHUP, 0, NULL,
                             constrained_extensions_assign_hook, NULL);

  DefineCustomStringVariable("supautils.drop_trigger_grants",
                             "Allow non-owners to drop triggers on tables",
                             NULL, &drop_trigger_grants_str, NULL, PGC_SIGHUP,
                             0, &drop_trigger_grants_check_hook, NULL, NULL);

  DefineCustomStringVariable("supautils.policy_grants",
                             "Allow non-owners to manage policies on tables",
                             NULL, &policy_grants_str, NULL, PGC_SIGHUP, 0,
                             &policy_grants_check_hook, NULL, NULL);

  DefineCustomBoolVariable("supautils.log_skipped_evtrigs",
                           "Log skipped event triggers with a NOTICE level",
                           NULL, &log_skipped_evtrigs, false, PGC_USERSET, 0,
                           NULL, NULL, NULL);

  DefineCustomBoolVariable(
      "supautils.disable_program", "Disable COPY TO/FROM PROGRAM", NULL,
      &disable_copy_program, false, PGC_SIGHUP, GUC_SUPERUSER_ONLY,
      disable_program_guc_check_hook, NULL, NULL);

  if (placeholders) {
    List     *comma_separated_list;
    ListCell *cell;

    SplitIdentifierString(pstrdup(placeholders), ',', &comma_separated_list);

    foreach (cell, comma_separated_list) {
      char *pholder = (char *)lfirst(cell);

      DefineCustomStringVariable(pholder, "", NULL, &empty_placeholder, NULL,
                                 PGC_USERSET, 0,
                                 restrict_placeholders_check_hook, NULL, NULL);
    }
    list_free(comma_separated_list);
  }

  EmitWarningsOnPlaceholders("supautils");
}

/*
 * This is just for completion. Right now postgres doesn't call _PG_fini, see:
 * https://github.com/postgres/postgres/blob/master/src/backend/utils/fmgr/dfmgr.c#L388-L402
 */
void _PG_fini(void) {
  ProcessUtility_hook = prev_hook;
}
