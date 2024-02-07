#include <postgres.h>

#include <access/xact.h>
#include <catalog/pg_authid.h>
#include <executor/spi.h>
#include <fmgr.h>
#include <miscadmin.h>
#include <nodes/makefuncs.h>
#include <nodes/pg_list.h>
#include <tcop/utility.h>
#include <tsearch/ts_locale.h>
#include <utils/acl.h>
#include <utils/builtins.h>
#include <utils/fmgrprotos.h>
#include <utils/guc.h>
#include <utils/guc_tables.h>
#include <utils/jsonb.h>
#include <utils/snapmgr.h>
#include <utils/varlena.h>

#include "constrained_extensions.h"
#include "extensions_parameter_overrides.h"
#include "privileged_extensions.h"
#include "utils.h"

#define EREPORT_RESERVED_MEMBERSHIP(name)									\
	ereport(ERROR,															\
			(errcode(ERRCODE_INSUFFICIENT_PRIVILEGE),						\
			 errmsg("\"%s\" role memberships are reserved, only superusers "\
					"can grant them", name)))

#define EREPORT_RESERVED_ROLE(name)											\
	ereport(ERROR,															\
			(errcode(ERRCODE_INSUFFICIENT_PRIVILEGE),						\
			 errmsg("\"%s\" is a reserved role, only superusers can modify "\
					"it", name)))

#define EREPORT_INVALID_PARAMETER(name)										\
	ereport(ERROR,															\
			(errcode(ERRCODE_INVALID_PARAMETER_VALUE),						\
			 errmsg("parameter \"%s\" must be a comma-separated list of "	\
					"identifiers", name)));

#define MAX_CONSTRAINED_EXTENSIONS         100
#define MAX_EXTENSIONS_PARAMETER_OVERRIDES 100

/* required macro for extension libraries to work */
PG_MODULE_MAGIC;

static char *reserved_roles                            = NULL;
static char *reserved_memberships                      = NULL;
static char *placeholders                              = NULL;
static char *placeholders_disallowed_values            = NULL;
static char *empty_placeholder                         = NULL;
static char *privileged_extensions                     = NULL;
static char *privileged_extensions_superuser           = NULL;
static char *privileged_extensions_custom_scripts_path = NULL;
static char *privileged_role                           = NULL;
static char *privileged_role_allowed_configs           = NULL;
static ProcessUtility_hook_type prev_hook              = NULL;

static char *constrained_extensions_str                = NULL;
static constrained_extension cexts[MAX_CONSTRAINED_EXTENSIONS] = {0};
static size_t total_cexts = 0;
static void
constrained_extensions_assign_hook(const char *newval, void *extra);

static char *extensions_parameter_overrides_str = NULL;
static extension_parameter_overrides epos[MAX_EXTENSIONS_PARAMETER_OVERRIDES] = {0};
static size_t total_epos = 0;
static bool
extensions_parameter_overrides_check_hook(char **newval, void **extra, GucSource source);

void _PG_init(void);
void _PG_fini(void);

static void
supautils_hook(PROCESS_UTILITY_PARAMS);

static void
confirm_reserved_roles(const char *target, bool allow_configurable_roles);

static void
confirm_reserved_memberships(const char *target);

static bool
reserved_roles_check_hook(char **newval, void **extra, GucSource source);

static bool
reserved_memberships_check_hook(char **newval, void **extra, GucSource source);

static bool
placeholders_check_hook(char **newval, void **extra, GucSource source);

static bool
placeholders_disallowed_values_check_hook(char **newval, void **extra, GucSource source);

static bool
restrict_placeholders_check_hook(char **newval, void **extra, GucSource source);

static bool
privileged_extensions_check_hook(char **newval, void **extra, GucSource source);

static bool
privileged_role_allowed_configs_check_hook(char **newval, void **extra, GucSource source);

static void check_parameter(char *val, char *name);

static bool
is_privileged_role(void);

void
_PG_init(void)
{
	/* Store the previous hook */
	prev_hook = ProcessUtility_hook;

	/* Set our hook */
	ProcessUtility_hook = supautils_hook;

	DefineCustomStringVariable("supautils.extensions_parameter_overrides",
							   "Overrides for CREATE EXTENSION parameters",
							   NULL,
							   &extensions_parameter_overrides_str,
							   NULL,
							   PGC_SIGHUP, 0,
							   &extensions_parameter_overrides_check_hook,
							   NULL,
							   NULL);

	DefineCustomStringVariable("supautils.reserved_roles",
							   "Comma-separated list of roles that cannot be modified",
							   NULL,
							   &reserved_roles,
							   NULL,
							   PGC_SIGHUP, 0,
							   reserved_roles_check_hook,
							   NULL,
							   NULL);

	DefineCustomStringVariable("supautils.reserved_memberships",
							   "Comma-separated list of roles whose memberships cannot be granted",
							   NULL,
							   &reserved_memberships,
							   NULL,
							   PGC_SIGHUP, 0,
							   reserved_memberships_check_hook,
							   NULL,
							   NULL);

	DefineCustomStringVariable("supautils.placeholders",
								 "GUC placeholders which will get values disallowed according to supautils.placeholders_disallowed_values",
								 NULL,
								 &placeholders,
								 NULL,
								 PGC_SIGHUP, 0,
								 placeholders_check_hook,
								 NULL,
								 NULL);

	DefineCustomStringVariable("supautils.placeholders_disallowed_values",
								 "disallowed values for the GUC placeholders defined in supautils.placeholders",
								 NULL,
								 &placeholders_disallowed_values,
								 NULL,
								 PGC_SIGHUP, 0,
								 placeholders_disallowed_values_check_hook,
								 NULL,
								 NULL);

	DefineCustomStringVariable("supautils.privileged_extensions",
							   "Comma-separated list of extensions which get installed using supautils.privileged_extensions_superuser",
							   NULL,
							   &privileged_extensions,
							   NULL,
							   PGC_SIGHUP, 0,
							   privileged_extensions_check_hook,
							   NULL,
							   NULL);

	DefineCustomStringVariable("supautils.privileged_extensions_custom_scripts_path",
							   "Path to load privileged extensions' custom scripts from",
							   NULL,
							   &privileged_extensions_custom_scripts_path,
							   NULL,
							   PGC_SIGHUP, 0,
							   NULL,
							   NULL,
							   NULL);

	DefineCustomStringVariable("supautils.privileged_extensions_superuser",
							   "Superuser to install extensions in supautils.privileged_extensions as",
							   NULL,
							   &privileged_extensions_superuser,
							   NULL,
							   PGC_SIGHUP, 0,
							   NULL,
							   NULL,
							   NULL);

	DefineCustomStringVariable("supautils.privileged_role",
							   "Non-superuser role to be granted with additional privileges",
							   NULL,
							   &privileged_role,
							   NULL,
							   PGC_SIGHUP, 0,
							   NULL,
							   NULL,
							   NULL);

	DefineCustomStringVariable("supautils.privileged_role_allowed_configs",
							   "Superuser-only configs that the privileged_role is allowed to configure",
							   NULL,
							   &privileged_role_allowed_configs,
							   NULL,
							   PGC_SIGHUP, 0,
							   privileged_role_allowed_configs_check_hook,
							   NULL,
							   NULL);

	DefineCustomStringVariable("supautils.constrained_extensions",
							   "Extensions that require a minimum amount of CPUs, memory and free disk to be installed",
							   NULL,
							   &constrained_extensions_str,
							   NULL,
							   SUPAUTILS_GUC_CONTEXT_SIGHUP, 0,
							   NULL,
							   constrained_extensions_assign_hook,
							   NULL);

	if(placeholders){
		List* comma_separated_list;
		ListCell* cell;

		SplitIdentifierString(pstrdup(placeholders), ',', &comma_separated_list);

		foreach(cell, comma_separated_list)
		{
			char *pholder = (char *) lfirst(cell);

			DefineCustomStringVariable(pholder,
										 "",
										 NULL,
										 &empty_placeholder,
										 NULL,
										 PGC_USERSET, 0,
										 restrict_placeholders_check_hook,
										 NULL,
										 NULL);
		}
		list_free(comma_separated_list);
	}

	EmitWarningsOnPlaceholders("supautils");
}

/*
 * IO: module unload callback
 * This is just for completion. Right now postgres doesn't call _PG_fini, see:
 * https://github.com/postgres/postgres/blob/master/src/backend/utils/fmgr/dfmgr.c#L388-L402
 */
void
_PG_fini(void)
{
	ProcessUtility_hook = prev_hook;
}

/*
 * IO: run the hook logic
 */
static void
supautils_hook(PROCESS_UTILITY_PARAMS)
{
	/* Get the utility statement from the planned statement */
	Node   *utility_stmt = pstmt->utilityStmt;

	switch (utility_stmt->type)
	{
		/*
		 * ALTER ROLE <role> NOLOGIN NOINHERIT..
		 */
		case T_AlterRoleStmt:
			{
				AlterRoleStmt *stmt = (AlterRoleStmt *)utility_stmt;
				DefElem *bypassrls_option = NULL;
				DefElem *replication_option = NULL;
				List *deferred_options = NIL;
				ListCell *option_cell = NULL;

				if (!IsTransactionState()) {
					break;
				}
				if (superuser()) {
					break;
				}

				confirm_reserved_roles(get_rolespec_name(stmt->role), false);

				if (!is_privileged_role()){
					break;
				}

				/* Check to see if there are any descriptions related to
				 * bypassrls or replication. */
				foreach(option_cell, stmt->options)
				{
					DefElem *defel = (DefElem *) lfirst(option_cell);

					if (strcmp(defel->defname, "bypassrls") == 0) {
						if (bypassrls_option != NULL) {
							ereport(ERROR,
									(errcode(ERRCODE_SYNTAX_ERROR),
									 errmsg("conflicting or redundant options")));
						}
						bypassrls_option = defel;
					}

					if (strcmp(defel->defname, "isreplication") == 0) {
						if (replication_option != NULL) {
							ereport(ERROR,
									(errcode(ERRCODE_SYNTAX_ERROR),
									 errmsg("conflicting or redundant options")));
						}
						replication_option = defel;
					}
				}

				// Defer setting bypassrls or replication attributes if using
				// `privileged_role`.
				if (bypassrls_option != NULL) {
					stmt->options = list_delete_ptr(stmt->options, bypassrls_option);
					deferred_options = lappend(deferred_options, bypassrls_option);
				}
				if (replication_option != NULL) {
					stmt->options = list_delete_ptr(stmt->options, replication_option);
					deferred_options = lappend(deferred_options, replication_option);
				}

				run_process_utility_hook(prev_hook);

				if (deferred_options != NIL) {
					alter_role_with_options_as_superuser(stmt->role->rolename, deferred_options, privileged_extensions_superuser);
				}

				return;
			}

		/*
		 * ALTER ROLE <role> SET search_path TO ...
		 */
		case T_AlterRoleSetStmt:
			{
				AlterRoleSetStmt *stmt = (AlterRoleSetStmt *)utility_stmt;
				bool role_is_privileged = false;

				if (!IsTransactionState()) {
					break;
				}
				if (superuser()) {
					break;
				}

				role_is_privileged = is_privileged_role();

				confirm_reserved_roles(get_rolespec_name(stmt->role), role_is_privileged);

				if (!role_is_privileged){
					break;
				}

				if (privileged_role_allowed_configs == NULL) {
					break;
				} else {
					bool is_privileged_role_allowed_config = is_string_in_comma_delimited_string(((VariableSetStmt *)stmt->setstmt)->name, privileged_role_allowed_configs);

					if (!is_privileged_role_allowed_config) {
						break;
					}
				}

				{
					bool already_switched_to_superuser = false;
					switch_to_superuser(privileged_extensions_superuser, &already_switched_to_superuser);

					run_process_utility_hook(prev_hook);

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
		case T_CreateRoleStmt:
			{
				if (IsTransactionState() && !superuser())
				{
					CreateRoleStmt *stmt = (CreateRoleStmt *) utility_stmt;
					const char *created_role = stmt->role;
					List *addroleto = NIL;	/* roles to make this a member of */
					bool hasrolemembers = false;	/* has roles to be members of this role */
					List *deferred_options = NIL;
					bool stmt_has_bypassrls = false;
					bool stmt_has_replication = false;
					bool role_is_privileged = is_privileged_role();
					ListCell *option_cell;

					/* if role already exists, bypass the hook to let it fail with the usual error */
					if (OidIsValid(get_role_oid(created_role, true)))
						break;

					/* CREATE ROLE <reserved_role> */
					confirm_reserved_roles(created_role, false);

					/* Check to see if there are any descriptions related to membership and bypassrls. */
					foreach(option_cell, stmt->options)
					{
						DefElem *defel = (DefElem *) lfirst(option_cell);
						if (strcmp(defel->defname, "addroleto") == 0)
							addroleto = (List *) defel->arg;

						if (strcmp(defel->defname, "rolemembers") == 0 ||
							strcmp(defel->defname, "adminmembers") == 0)
							hasrolemembers = true;

						// Defer setting bypassrls & replication attributes if
						// using `privileged_role`.
						//
						// Duplicate/conflicting attributes will be caught by
						// the standard process utility hook, so we can assume
						// there's at most one bypassrls DefElem.
						if (role_is_privileged && strcmp(defel->defname, "bypassrls") == 0) {
							stmt_has_bypassrls = intVal(defel->arg) != 0;
							intVal(defel->arg) = 0;
						}
						if (role_is_privileged && strcmp(defel->defname, "isreplication") == 0) {
							stmt_has_replication = intVal(defel->arg) != 0;
							intVal(defel->arg) = 0;
						}
					}

					/* CREATE ROLE <any_role> IN ROLE/GROUP <role_with_reserved_membership> */
					if (addroleto)
					{
						ListCell *role_cell;
						foreach(role_cell, addroleto)
						{
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
					if (hasrolemembers)
						confirm_reserved_memberships(created_role);

					// CREATE ROLE <any_role> < BYPASSRLS | REPLICATION >
					//
					// Allow `privileged_role` (in addition to superusers) to
					// set bypassrls and replication attributes. The setting of
					// the attributes is deferred in the original CREATE ROLE -
					// the actual setting is done here.
					if (role_is_privileged) {
						Node *true_node = (Node *)makeInteger(true);
						DefElem *bypassrls_option = makeDefElem("bypassrls", true_node, -1);
						DefElem *replication_option = makeDefElem("isreplication", true_node, -1);

						if (stmt_has_bypassrls) {
							deferred_options = lappend(deferred_options, bypassrls_option);
						}
						if (stmt_has_replication) {
							deferred_options = lappend(deferred_options, replication_option);
						}

						run_process_utility_hook(prev_hook);

						alter_role_with_options_as_superuser(stmt->role, deferred_options, privileged_extensions_superuser);

						pfree(true_node);
						pfree(bypassrls_option);
						pfree(replication_option);
						list_free(deferred_options);

						return;
					}
				}
				break;
			}

		/*
		 * DROP ROLE
		 */
		case T_DropRoleStmt:
			{
				if (IsTransactionState() && !superuser())
				{
					DropRoleStmt *stmt = (DropRoleStmt *) utility_stmt;
					ListCell *item;

					foreach(item, stmt->roles)
					{
						RoleSpec *role = lfirst(item);

						/*
						 * We check only for a named role being dropped; we ignore
						 * the special values like PUBLIC, CURRENT_USER, and
						 * SESSION_USER. We let Postgres throw its usual error messages
						 * for those special values.
						 */
						if (role->roletype != ROLESPEC_CSTRING)
							break;

						confirm_reserved_roles(role->rolename, false);
					}
				}
				break;
			}

		/*
		 * GRANT <role> and REVOKE <role>
		 */
		case T_GrantRoleStmt:
			{
				if (IsTransactionState() && !superuser())
				{
					GrantRoleStmt *stmt = (GrantRoleStmt *) utility_stmt;
					ListCell *grantee_role_cell;
					ListCell *role_cell;
					bool role_is_privileged = false;

					/* GRANT <reserved_role> TO <role> */
					if (stmt->is_grant)
					{
						foreach(role_cell, stmt->granted_roles)
						{
							AccessPriv *priv = (AccessPriv *) lfirst(role_cell);
							confirm_reserved_memberships(priv->priv_name);
						}
					}

					role_is_privileged = is_privileged_role();

					/*
					 * GRANT <role> TO <reserved_roles>
					 * REVOKE <role> FROM <reserved_roles>
					 */
					foreach(grantee_role_cell, stmt->grantee_roles)
					{
						AccessPriv *priv = (AccessPriv *) lfirst(grantee_role_cell);
						// privileged_role can do GRANT <role> to <reserved_role>
						confirm_reserved_roles(priv->priv_name, role_is_privileged);
					}
				}
				break;
			}

		/*
		 * All RENAME statements are caught here
		 */
		case T_RenameStmt:
			{
				if (IsTransactionState() && !superuser())
				{
					RenameStmt *stmt = (RenameStmt *) utility_stmt;

					/* Make sure we only catch "ALTER ROLE <role> RENAME TO" */
					if (stmt->renameType != OBJECT_ROLE)
						break;

					confirm_reserved_roles(stmt->subname, false);
					confirm_reserved_roles(stmt->newname, false);
				}
				break;
			}

		/*
		 * CREATE EXTENSION <extension>
		 */
		case T_CreateExtensionStmt:
		{

			CreateExtensionStmt *stmt = (CreateExtensionStmt *)utility_stmt;

			constrain_extension(stmt->extname, cexts, total_cexts);

			handle_create_extension(prev_hook,
									PROCESS_UTILITY_ARGS,
									privileged_extensions,
									privileged_extensions_superuser,
									privileged_extensions_custom_scripts_path,
                                    epos, total_epos);
			return;
        }

		/*
		 * ALTER EXTENSION <extension> [ UPDATE | SET SCHEMA ]
		 */
		case T_AlterExtensionStmt:
		{
			if (superuser()) {
				break;
			}
			if (privileged_extensions == NULL) {
				break;
			}

			handle_alter_extension(prev_hook,
								   PROCESS_UTILITY_ARGS,
								   privileged_extensions,
								   privileged_extensions_superuser);
			return;
		}

		/*
		 * ALTER EXTENSION <extension> [ ADD | DROP ]
		 *
		 * Not supported. Fall back to normal behavior.
		 */
		case T_AlterExtensionContentsStmt:
			break;

		/**
		 * CREATE FOREIGN DATA WRAPPER <fdw>
		 */
		case T_CreateFdwStmt:
		{
			const Oid current_user_id = GetUserId();
			bool already_switched_to_superuser = false;

			if (superuser()) {
				break;
			}
			if (!is_privileged_role()) {
				break;
			}

			switch_to_superuser(privileged_extensions_superuser, &already_switched_to_superuser);

			run_process_utility_hook(prev_hook);

			// Change FDW owner to the current role (which is a privileged role)
			{
				CreateFdwStmt *stmt = (CreateFdwStmt *)utility_stmt;

				const char *current_role_name = GetUserNameFromId(current_user_id, false);
				const char *current_role_name_ident = quote_identifier(current_role_name);
				const char *fdw_name_ident = quote_identifier(stmt->fdwname);
				// Need to temporarily make the current role a superuser because non SUs can't own FDWs.
				char *sql_template = "alter role %s superuser;\n"
					"alter foreign data wrapper %s owner to %s;\n"
					"alter role %s nosuperuser;\n";
				size_t sql_len = strlen(sql_template)
					+ (3 * strlen(current_role_name_ident))
					+ strlen(fdw_name_ident);
				char *sql = (char *)palloc(sql_len);
				int rc;

				PushActiveSnapshot(GetTransactionSnapshot());
				SPI_connect();

				snprintf(sql,
						 sql_len,
						 sql_template,
						 current_role_name_ident,
						 fdw_name_ident,
						 current_role_name_ident,
						 current_role_name_ident);

				rc = SPI_execute(sql, false, 0);
				if (rc != SPI_OK_UTILITY) {
					elog(ERROR, "SPI_execute failed with error code %d", rc);
				}

				pfree(sql);

				SPI_finish();
				PopActiveSnapshot();
			}

			if (!already_switched_to_superuser) {
				switch_to_original_role();
			}

			return;
		}

		/**
		 * CREATE PUBLICATION
		 */
		case T_CreatePublicationStmt:
		{
			const Oid current_user_id = GetUserId();
			bool already_switched_to_superuser = false;

			if (superuser()) {
				break;
			}
			if (!is_privileged_role()) {
				break;
			}

			switch_to_superuser(privileged_extensions_superuser, &already_switched_to_superuser);

			run_process_utility_hook(prev_hook);

			// Change publication owner to the current role (which is a privileged role)
			{
				CreatePublicationStmt *stmt = (CreatePublicationStmt *)utility_stmt;

				const char *current_role_name = GetUserNameFromId(current_user_id, false);
				const char *current_role_name_ident = quote_identifier(current_role_name);
				const char *publication_name_ident = quote_identifier(stmt->pubname);
				const char *sql_template = "alter publication %s owner to %s;\n";
				size_t sql_len = strlen(sql_template)
					+ strlen(publication_name_ident)
					+ strlen(current_role_name_ident);
				char *sql = (char *)palloc(sql_len);
				int rc;

				snprintf(sql,
						 sql_len,
						 sql_template,
						 publication_name_ident,
						 current_role_name_ident);

				PushActiveSnapshot(GetTransactionSnapshot());
				SPI_connect();

				rc = SPI_execute(sql, false, 0);
				if (rc != SPI_OK_UTILITY) {
					elog(ERROR, "SPI_execute failed with error code %d", rc);
				}

				pfree(sql);

				SPI_finish();
				PopActiveSnapshot();
			}

			if (!already_switched_to_superuser) {
				switch_to_original_role();
			}

			return;
		}

		/**
		 * ALTER PUBLICATION <name> ADD TABLES IN SCHEMA ...
		 */
		case T_AlterPublicationStmt:
		{
			bool already_switched_to_superuser = false;

			if (superuser()) {
				break;
			}
			if (!is_privileged_role()) {
				break;
			}

			switch_to_superuser(privileged_extensions_superuser, &already_switched_to_superuser);

			run_process_utility_hook(prev_hook);

			if (!already_switched_to_superuser) {
				switch_to_original_role();
			}

			return;
		}

		case T_DropStmt:
		{
			DropStmt *stmt = (DropStmt *)utility_stmt;

			if (superuser()) {
				break;
			}
			if (privileged_extensions == NULL) {
				break;
			}

			/*
			 * DROP EXTENSION <extension>
			 */
			if (stmt->removeType == OBJECT_EXTENSION) {
				handle_drop_extension(prev_hook,
									  PROCESS_UTILITY_ARGS,
									  privileged_extensions,
									  privileged_extensions_superuser);
				return;
			}

			break;
		}

		case T_CommentStmt:
		{
			if (!IsTransactionState()) {
				break;
			}
			if (superuser()) {
				break;
			}
			if (((CommentStmt *)utility_stmt)->objtype != OBJECT_EXTENSION) {
				break;
			}
			if (!is_privileged_role()) {
				break;
			}

			{
				bool already_switched_to_superuser = false;
				switch_to_superuser(privileged_extensions_superuser, &already_switched_to_superuser);

				run_process_utility_hook(prev_hook);

		     	if (!already_switched_to_superuser) {
		     		switch_to_original_role();
		     	}

				return;
			}
		}

		case T_VariableSetStmt:
		{
			if (!IsTransactionState()) {
				break;
			}
			if (superuser()) {
				break;
			}
			if (privileged_role_allowed_configs == NULL) {
				break;
			} else {
				bool is_privileged_role_allowed_config = is_string_in_comma_delimited_string(((VariableSetStmt *)utility_stmt)->name, privileged_role_allowed_configs);

				if (!is_privileged_role_allowed_config) {
					break;
				}
			}
			if (!is_privileged_role()) {
				break;
			}

			{
				bool already_switched_to_superuser = false;
				switch_to_superuser(privileged_extensions_superuser, &already_switched_to_superuser);

				run_process_utility_hook(prev_hook);

		     	if (!already_switched_to_superuser) {
		     		switch_to_original_role();
		     	}

				return;
			}
		}

		default:
			break;
	}

	/* Chain to previously defined hooks */
	run_process_utility_hook(prev_hook);
}

static bool
extensions_parameter_overrides_check_hook(char **newval, void **extra, GucSource source)
{
	char *val = *newval;

	if (total_epos > 0) {
		for (size_t i = 0; i < total_epos; i++){
			pfree(epos[i].name);
			pfree(epos[i].schema);
		}
		total_epos = 0;
	}

	if (val) {
		json_extension_parameter_overrides_parse_state state = parse_extensions_parameter_overrides(val, epos);
		if (state.error_msg) {
			ereport(ERROR,
					(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
					 errmsg("supautils.extensions_parameter_overrides: %s", state.error_msg)));
		}
		total_epos = state.total_epos;
	}

	return true;
}

static bool
reserved_roles_check_hook(char **newval, void **extra, GucSource source)
{
	check_parameter(*newval, "supautils.reserved_roles");

	return true;
}

static bool
reserved_memberships_check_hook(char **newval, void **extra, GucSource source)
{
	check_parameter(*newval, "supautils.reserved_memberships");

	return true;
}

static bool
placeholders_disallowed_values_check_hook(char **newval, void **extra, GucSource source)
{
	check_parameter(*newval, "supautils.placeholders_disallowed_values");

	return true;
}

static bool
privileged_extensions_check_hook(char **newval, void **extra, GucSource source)
{
	check_parameter(*newval, "supautils.privileged_extensions");

	return true;
}

static bool
privileged_role_allowed_configs_check_hook(char **newval, void **extra, GucSource source)
{
	check_parameter(*newval, "supautils.privileged_role_allowed_configs");

	return true;
}

static void
check_parameter(char *val, char *name)
{
	List *comma_separated_list;

	if (val != NULL)
	{
		if (!SplitIdentifierString(pstrdup(val), ',', &comma_separated_list))
			EREPORT_INVALID_PARAMETER(name);

		list_free(comma_separated_list);
	}
}

void constrained_extensions_assign_hook(const char *newval, void *extra){
	if (total_cexts > 0) {
		for (size_t i = 0; i < total_cexts; i++){
			pfree(cexts[i].name);
		}
		total_cexts = 0;
	}
	if (newval) {
		json_constrained_extension_parse_state state = parse_constrained_extensions(newval, cexts);
		total_cexts = state.total_cexts;
		if(state.error_msg){
			ereport(ERROR,															\
					(errcode(ERRCODE_INVALID_PARAMETER_VALUE),						\
					 errmsg("supautils.constrained_extensions: %s", state.error_msg)));
		}
	}
}

static void
confirm_reserved_roles(const char *target, bool allow_configurable_roles)
{
	List *reserved_roles_list;
	ListCell *role;

	if (reserved_roles)
	{
		SplitIdentifierString(pstrdup(reserved_roles), ',', &reserved_roles_list);

		foreach(role, reserved_roles_list)
		{
			char *reserved_role = (char *) lfirst(role);
			bool is_configurable_role = remove_ending_wildcard(reserved_role);
			bool should_modify_role = is_configurable_role && allow_configurable_roles;

			if (strcmp(target, reserved_role) == 0)
			{
				if(should_modify_role){
					continue;
				} else {
					list_free(reserved_roles_list);
					EREPORT_RESERVED_ROLE(reserved_role);
				}
			}
		}
		list_free(reserved_roles_list);
	}
}

static void
confirm_reserved_memberships(const char *target)
{
	List *reserved_memberships_list;
	ListCell *membership;

	if (reserved_memberships)
	{
		SplitIdentifierString(pstrdup(reserved_memberships), ',', &reserved_memberships_list);

		foreach(membership, reserved_memberships_list)
		{
			char *reserved_membership = (char *) lfirst(membership);

			if (strcmp(target, reserved_membership) == 0)
			{
				list_free(reserved_memberships_list);
				EREPORT_RESERVED_MEMBERSHIP(reserved_membership);
			}
		}
		list_free(reserved_memberships_list);
	}
}

static bool
placeholders_check_hook(char **newval, void **extra, GucSource source)
{
	char* val = *newval;

	if (val)
	{
		List* comma_separated_list;
		ListCell* cell;
		bool saw_sep = false;

		if(!SplitIdentifierString(pstrdup(val), ',', &comma_separated_list))
			EREPORT_INVALID_PARAMETER("supautils.placeholders");

		foreach(cell, comma_separated_list)
		{
			for (const char *p = lfirst(cell); *p; p++)
			{
				// check if the GUC has a "." in it(if it's a placeholder)
				if (*p == GUC_QUALIFIER_SEPARATOR)
					saw_sep = true;
			}
		}

		list_free(comma_separated_list);

		if(!saw_sep)
			ereport(ERROR,															\
					(errcode(ERRCODE_INVALID_PARAMETER_VALUE),						\
					 errmsg("supautils.placeholders must contain guc placeholders")));

	}

	return true;
}

static bool
restrict_placeholders_check_hook(char **newval, void **extra, GucSource source)
{
	bool not_empty = placeholders_disallowed_values && placeholders_disallowed_values[0] != '\0';

	if(*newval && not_empty)
	{
		char *token, *string, *tofree;
		char *val = lowerstr(*newval);

		tofree = string = pstrdup(placeholders_disallowed_values);

		while( (token = strsep(&string, ",")) != NULL )
		{
			if (strstr(val, token))
			{
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

static bool
is_privileged_role(void)
{
	Oid current_role_oid = GetUserId();
	Oid	privileged_role_oid;

	if (privileged_role == NULL) {
		return false;
	}
	privileged_role_oid = get_role_oid(privileged_role, true);

	return OidIsValid(privileged_role_oid) && is_member_of_role(current_role_oid, privileged_role_oid);
}
