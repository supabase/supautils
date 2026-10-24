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
                      defGetString(defel))));
    else
      ereport(WARNING,
              (errmsg("only superusers can specify extension versions, "
                      "ignoring version \"%s\" and updating to the default "
                      "version",
                      defGetString(defel))));

    options = foreach_delete_current(options, lc);
  }

  return options;
}

/*
 * CREATE EXTENSION <extension>
 */
static bool create_extension(char *extname, List **options,
                             const utility_hook_args *args,
                             const extension_policy  *policy) {
  // volatile is not needed for correctness: the variable is assigned before
  // the PG_TRY inside RUN_ELEVATED and never read in the catch path. it only
  // silences gcc's conservative -Wclobbered, which errors under -Werror in CI
  volatile int save_nestlevel = 0;

  *options = restrict_version_specification(EXT_CREATE, *options, policy);

  // prepend the default schema to search_path so schema-less creates land
  // there. pinned control files and explicit SCHEMA clauses still win since
  // postgres only falls back to search_path. saved and restored via a GUC
  // nest level, same as core's execute_extension_script, so on error the
  // change is rolled back with the transaction.
  if (policy->default_schema != NULL && policy->default_schema[0] != '\0' &&
      OidIsValid(get_namespace_oid(policy->default_schema, true))) {
    const char *current = GetConfigOption("search_path", true, false);

    save_nestlevel = NewGUCNestLevel();
    (void)set_config_option(
        "search_path",
        (current == NULL || current[0] == '\0')
            ? quote_identifier(policy->default_schema)
            : psprintf("%s, %s", quote_identifier(policy->default_schema),
                       current),
        PGC_USERSET, PGC_S_SESSION, GUC_ACTION_SAVE, true, 0, false);
  }

  RUN_ELEVATED(policy->superuser,

               run_global_before_create_script(extname, *options,
                                               policy->custom_scripts_path);

               run_ext_before_create_script(extname, *options,
                                            policy->custom_scripts_path);

               *options = override_ext_options(EXT_CREATE, extname, *options,
                                               policy->total_overrides,
                                               policy->overrides));

  if (is_extension_privileged(extname, policy->privileged_extensions)) {
    RUN_ELEVATED(policy->superuser, run_prev_utility_hook(args));
  } else {
    // non-privileged extensions are created as the caller
    run_prev_utility_hook(args);
  }

  RUN_ELEVATED(policy->superuser,
               run_ext_after_create_script(extname, *options,
                                           policy->custom_scripts_path);
               run_global_after_create_script(extname, *options,
                                              policy->custom_scripts_path));

  if (save_nestlevel > 0) {
    AtEOXact_GUC(true, save_nestlevel);
  }

  return true;
}

/*
 * ALTER EXTENSION <extension> [ UPDATE ]
 */
static bool alter_extension(char *extname, List **options,
                            const utility_hook_args *args,
                            const extension_policy  *policy) {
  if (superuser()) {
    return false;
  }

  *options = restrict_version_specification(EXT_ALTER, *options, policy);

  *options = override_ext_options(EXT_ALTER, extname, *options,
                                  policy->total_overrides, policy->overrides);

  if (!is_extension_privileged(extname, policy->privileged_extensions)) {
    return false;
  }

  RUN_ELEVATED(policy->superuser, run_prev_utility_hook(args));

  return true;
}

/*
 * ALTER EXTENSION <extension> SET SCHEMA
 */
static bool alter_extension_schema(const char              *extname,
                                   const utility_hook_args *args,
                                   const extension_policy  *policy) {
  if (superuser()) {
    return false;
  }
  if (!is_extension_privileged(extname, policy->privileged_extensions)) {
    return false;
  }

  RUN_ELEVATED(policy->superuser, run_prev_utility_hook(args));

  return true;
}

/*
 * DROP EXTENSION <extension>
 */
static bool drop_extension(List *extnames, const utility_hook_args *args,
                           const extension_policy *policy) {
  if (superuser()) {
    return false;
  }
  if (!all_extensions_are_privileged(extnames, policy->privileged_extensions)) {
    return false;
  }

  RUN_ELEVATED(policy->superuser, run_prev_utility_hook(args));

  return true;
}

/*
 * COMMENT ON EXTENSION <extension>
 */
static bool comment_on_extension(const utility_hook_args *args,
                                 const extension_policy  *policy) {
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

/*
 * Handlers never see a parse node: the switch below reads the few fields the
 * policy needs and hands over plain values. CREATE/ALTER EXTENSION get a
 * pointer to the options list because they rewrite it before core runs.
 */
bool handle_extension_stmt(Node *stmt, const utility_hook_args *args,
                           const extension_policy *policy) {
  switch (nodeTag(stmt)) {
  case T_CreateExtensionStmt: {
    CreateExtensionStmt *create = (CreateExtensionStmt *)stmt;
    return create_extension(create->extname, &create->options, args, policy);
  }
  case T_AlterExtensionStmt: {
    AlterExtensionStmt *alter = (AlterExtensionStmt *)stmt;
    return alter_extension(alter->extname, &alter->options, args, policy);
  }
  case T_AlterObjectSchemaStmt: {
    AlterObjectSchemaStmt *schema = (AlterObjectSchemaStmt *)stmt;
    if (schema->objectType != OBJECT_EXTENSION) return false;
    return alter_extension_schema(strVal(schema->object), args, policy);
  }
  case T_DropStmt: {
    DropStmt *drop = (DropStmt *)stmt;
    if (drop->removeType != OBJECT_EXTENSION) return false;
    return drop_extension(drop->objects, args, policy);
  }
  case T_CommentStmt:
    if (((CommentStmt *)stmt)->objtype != OBJECT_EXTENSION) return false;
    return comment_on_extension(args, policy);
  default: return false;
  }
}
