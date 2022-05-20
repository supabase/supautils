#include <postgres.h>

#include <access/xact.h>
#include <fmgr.h>
#include <miscadmin.h>
#include <tcop/utility.h>
#include <tsearch/ts_locale.h>
#include <utils/acl.h>
#include <utils/guc.h>
#include <utils/guc_tables.h>
#include <utils/varlena.h>
#include <utils/jsonb.h>
#include <utils/fmgrprotos.h>

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
static ProcessUtility_hook_type prev_hook              = NULL;

void _PG_init(void);
void _PG_fini(void);

static void
supautils_hook(PROCESS_UTILITY_PARAMS);

static void
comfirm_reserved_roles(const char *target);

static void
comfirm_reserved_memberships(const char *target);

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

static void check_parameter(char *val, char *name);

/*
 * IO: module load callback
 */
void
_PG_init(void)
{
	/* Store the previous hook */
	prev_hook = ProcessUtility_hook;

	/* Set our hook */
	ProcessUtility_hook = supautils_hook;

	DefineCustomStringVariable("supautils.reserved_roles",
							   NULL,
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

	DefineCustomStringVariable("supautils.privileged_extensions_superuser",
							   "Superuser to install extensions in supautils.privileged_extensions as",
							   NULL,
							   &privileged_extensions_superuser,
							   NULL,
							   PGC_SIGHUP, 0,
							   NULL,
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
		 * ALTER ROLE <role> SET search_path TO ...
		 */
		case T_AlterRoleStmt:
		case T_AlterRoleSetStmt:
			{
				if (IsTransactionState() && !superuser())
				{
					AlterRoleStmt *stmt = (AlterRoleStmt *) utility_stmt;
					comfirm_reserved_roles(get_rolespec_name(stmt->role));
				}
				break;
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
					ListCell *option_cell;

					/* if role already exists, bypass the hook to let it fail with the usual error */
					if (OidIsValid(get_role_oid(created_role, true)))
						break;

					/* CREATE ROLE <reserved_role> */
					comfirm_reserved_roles(created_role);

					/* Check to see if there are any descriptions related to membership. */
					foreach(option_cell, stmt->options)
					{
						DefElem *defel = (DefElem *) lfirst(option_cell);
						if (strcmp(defel->defname, "addroleto") == 0)
							addroleto = (List *) defel->arg;

						if (strcmp(defel->defname, "rolemembers") == 0 ||
							strcmp(defel->defname, "adminmembers") == 0)
							hasrolemembers = true;
					}

					/* CREATE ROLE <any_role> IN ROLE/GROUP <role_with_reserved_membership> */
					if (addroleto)
					{
						ListCell *role_cell;
						foreach(role_cell, addroleto)
						{
							RoleSpec *rolemember = lfirst(role_cell);
							comfirm_reserved_memberships(get_rolespec_name(rolemember));
						}
					}

					/*
					 * CREATE ROLE <role_with_reserved_membership> ROLE/ADMIN/USER <any_role>
					 *
					 * This is a contrived case because the "role_with_reserved_membership"
					 * should already exist, but handle it anyway.
					 */
					if (hasrolemembers)
						comfirm_reserved_memberships(created_role);
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

						comfirm_reserved_roles(role->rolename);
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

					/* GRANT <role> TO <another_role> */
					if (stmt->is_grant)
					{
						foreach(role_cell, stmt->granted_roles)
						{
							AccessPriv *priv = (AccessPriv *) lfirst(role_cell);
							comfirm_reserved_memberships(priv->priv_name);
						}
					}

					/*
					 * GRANT <role> TO <reserved_roles>
					 * REVOKE <role> FROM <reserved_roles>
					 */
					foreach(grantee_role_cell, stmt->grantee_roles)
					{
						AccessPriv *priv = (AccessPriv *) lfirst(grantee_role_cell);
						comfirm_reserved_roles(priv->priv_name);
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

					comfirm_reserved_roles(stmt->subname);
					comfirm_reserved_roles(stmt->newname);
				}
				break;
			}

		/*
		 * CREATE EXTENSION <extension>
		 */
		case T_CreateExtensionStmt:
		{
			if (superuser()) {
				break;
			}
			if (privileged_extensions == NULL) {
				break;
			}

			handle_create_extension(prev_hook,
									PROCESS_UTILITY_ARGS,
									privileged_extensions,
									privileged_extensions_superuser,
									privileged_extensions_custom_scripts_path);
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
								   privileged_extensions_superuser,
								   privileged_extensions_custom_scripts_path);
			return;
		}

		/*
		 * ALTER EXTENSION <extension> [ ADD | DROP ]
		 *
		 * Not supported. Fall back to normal behavior.
		 */
		case T_AlterExtensionContentsStmt:
			break;

		case T_DropStmt:
		{
			if (superuser()) {
				break;
			}
			if (privileged_extensions == NULL) {
				break;
			}

			/*
			 * DROP EXTENSION <extension>
			 */
			if (((DropStmt *)utility_stmt)->removeType == OBJECT_EXTENSION) {
				handle_drop_extension(prev_hook,
									  PROCESS_UTILITY_ARGS,
									  privileged_extensions,
									  privileged_extensions_superuser,
									  privileged_extensions_custom_scripts_path);
				return;
			}

			break;
		}

		default:
			break;
	}

	/* Chain to previously defined hooks */
	run_process_utility_hook(prev_hook);
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

static void
comfirm_reserved_roles(const char *target)
{
	List *reserved_roles_list;
	ListCell *role;

	if (reserved_roles)
	{
		SplitIdentifierString(pstrdup(reserved_roles), ',', &reserved_roles_list);

		foreach(role, reserved_roles_list)
		{
			char *reserved_role = (char *) lfirst(role);

			if (strcmp(target, reserved_role) == 0)
			{
				list_free(reserved_roles_list);
				EREPORT_RESERVED_ROLE(reserved_role);
			}
		}
		list_free(reserved_roles_list);
	}
}

static void
comfirm_reserved_memberships(const char *target)
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
	char* val = *newval;

	if(val && placeholders_disallowed_values){
		List* comma_separated_list;
		ListCell* cell;

		SplitIdentifierString(pstrdup(placeholders_disallowed_values), ',', &comma_separated_list);

		foreach(cell, comma_separated_list)
		{
			char* disallowed_value = (char *) lfirst(cell);
			if (strstr(lowerstr(val), disallowed_value))
			{
				list_free(comma_separated_list);
				ereport(ERROR,															\
						(errcode(ERRCODE_INVALID_PARAMETER_VALUE),						\
						 errmsg("The placeholder contains the \"%s\" disallowed value", disallowed_value)));
			}
		}
		list_free(comma_separated_list);
	}

	return true;
}
