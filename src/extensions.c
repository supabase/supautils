#include "extensions.h"

#include "extension_custom_scripts.h"
#include "privileged_extensions.h"
#include "utils.h"

static List *restrict_version_specification(extension_stmt_kind     stmt_kind,
                                            List                   *options,
                                            const extension_policy *policy) {
  ListCell *lc;

  if (policy->restrict_versions == RESTRICT_EXTENSION_VERSIONS_OFF)
    return options;

  if (superuser()) return options;

  if (policy->superuser != NULL && policy->superuser[0] != '\0') {
    const char *current_user = GetUserNameFromId(GetUserId(), false);
    if (strcmp(current_user, policy->superuser) == 0) return options;
  }

  foreach (lc, options) {
    DefElem *defel = (DefElem *)lfirst(lc);

    if (strcmp(defel->defname, "new_version") != 0) continue;

    if (policy->restrict_versions == RESTRICT_EXTENSION_VERSIONS_ERROR) {
      if (stmt_kind == EXT_CREATE)
        ereport(ERROR,
                (errcode(ERRCODE_INSUFFICIENT_PRIVILEGE),
                 errmsg("permission denied: only superusers can specify "
                        "extension versions. Use CREATE EXTENSION <name> "
                        "without a VERSION clause.")));
      else
        ereport(ERROR, (errcode(ERRCODE_INSUFFICIENT_PRIVILEGE),
                        errmsg("permission denied: only superusers can specify "
                               "extension versions. Use ALTER EXTENSION <name> "
                               "UPDATE without a TO clause.")));
    }

    // warn mode: drop the version option so the default version is used
    if (stmt_kind == EXT_CREATE)
      ereport(WARNING,
              (errmsg("only superusers can specify extension versions, "
                      "ignoring version \"%s\" and installing the default "
                      "version",
                      strVal(defel->arg))));
    else
      ereport(WARNING,
              (errmsg("only superusers can specify extension versions, "
                      "ignoring version \"%s\" and updating to the default "
                      "version",
                      strVal(defel->arg))));

    options = foreach_delete_current(options, lc);
  }

  return options;
}

/*
 * CREATE EXTENSION <extension>
 */
static bool create_extension(CreateExtensionStmt     *stmt,
                             const utility_hook_args *args,
                             const extension_policy  *policy) {

  if (!is_current_role_privileged(policy->privileged_role) && !superuser()) {
    return false;
  }

  stmt->options =
      restrict_version_specification(EXT_CREATE, stmt->options, policy);

  constrain_extension(stmt->extname, policy->constrained,
                      policy->total_constrained);

  RUN_ELEVATED(policy->superuser,

               run_global_before_create_script(stmt->extname, stmt->options,
                                               policy->custom_scripts_path);

               run_ext_before_create_script(stmt->extname, stmt->options,
                                            policy->custom_scripts_path);

               stmt->options = override_ext_options(
                   EXT_CREATE, stmt->extname, stmt->options,
                   policy->total_overrides, policy->overrides));

  if (is_extension_privileged(stmt->extname, policy->privileged_extensions)) {
    RUN_ELEVATED(policy->superuser, run_prev_utility_hook(args));
  } else {
    // non-privileged extensions are created as the caller
    run_prev_utility_hook(args);
  }

  RUN_ELEVATED(policy->superuser,
               run_ext_after_create_script(stmt->extname, stmt->options,
                                           policy->custom_scripts_path));

  return true;
}

/*
 * ALTER EXTENSION <extension> [ UPDATE ]
 */
static bool alter_extension(AlterExtensionStmt      *stmt,
                            const utility_hook_args *args,
                            const extension_policy  *policy) {
  if (superuser()) {
    return false;
  }

  stmt->options =
      restrict_version_specification(EXT_ALTER, stmt->options, policy);

  stmt->options =
      override_ext_options(EXT_ALTER, stmt->extname, stmt->options,
                           policy->total_overrides, policy->overrides);

  if (!is_extension_privileged(stmt->extname, policy->privileged_extensions)) {
    return false;
  }

  RUN_ELEVATED(policy->superuser, run_prev_utility_hook(args));

  return true;
}

/*
 * ALTER EXTENSION <extension> SET SCHEMA
 */
static bool alter_extension_schema(AlterObjectSchemaStmt   *stmt,
                                   const utility_hook_args *args,
                                   const extension_policy  *policy) {
  if (stmt->objectType != OBJECT_EXTENSION) {
    return false;
  }
  if (superuser()) {
    return false;
  }
  if (!is_extension_privileged(strVal(stmt->object),
                               policy->privileged_extensions)) {
    return false;
  }

  RUN_ELEVATED(policy->superuser, run_prev_utility_hook(args));

  return true;
}

/*
 * DROP EXTENSION <extension>
 */
static bool drop_extension(DropStmt *stmt, const utility_hook_args *args,
                           const extension_policy *policy) {
  if (stmt->removeType != OBJECT_EXTENSION) {
    return false;
  }
  if (superuser()) {
    return false;
  }
  if (!all_extensions_are_privileged(stmt->objects,
                                     policy->privileged_extensions)) {
    return false;
  }

  RUN_ELEVATED(policy->superuser, run_prev_utility_hook(args));

  return true;
}

/*
 * COMMENT ON EXTENSION <extension>
 */
static bool comment_on_extension(CommentStmt             *stmt,
                                 const utility_hook_args *args,
                                 const extension_policy  *policy) {
  if (stmt->objtype != OBJECT_EXTENSION) {
    return false;
  }
  if (!IsTransactionState()) {
    return false;
  }
  if (superuser()) {
    return false;
  }
  if (!is_current_role_privileged(policy->privileged_role)) {
    return false;
  }

  RUN_ELEVATED(policy->superuser, run_prev_utility_hook(args));

  return true;
}

bool handle_extension_stmt(Node *stmt, const utility_hook_args *args,
                           const extension_policy *policy) {
  switch (nodeTag(stmt)) {
  case T_CreateExtensionStmt:
    return create_extension((CreateExtensionStmt *)stmt, args, policy);
  case T_AlterExtensionStmt:
    return alter_extension((AlterExtensionStmt *)stmt, args, policy);
  case T_AlterObjectSchemaStmt:
    return alter_extension_schema((AlterObjectSchemaStmt *)stmt, args, policy);
  case T_DropStmt: return drop_extension((DropStmt *)stmt, args, policy);
  case T_CommentStmt:
    return comment_on_extension((CommentStmt *)stmt, args, policy);
  default: return false;
  }
}
