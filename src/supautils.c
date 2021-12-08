#include "postgres.h"

#include "access/xact.h"
#include "miscadmin.h"
#include "tcop/utility.h"
#include "utils/acl.h"
#include "utils/guc.h"
#include "utils/guc_tables.h"
#include "utils/varlena.h"

#define PG13_GTE (PG_VERSION_NUM >= 130000)
#define PG14_GTE (PG_VERSION_NUM >= 140000)

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

// required macro for extension libraries to work
PG_MODULE_MAGIC;

static char *reserved_roles					= NULL;
static char *reserved_memberships			= NULL;
static ProcessUtility_hook_type prev_hook	= NULL;

void _PG_init(void);
void _PG_fini(void);

static void
supautils_hook(PlannedStmt *pstmt,
				const char *queryString,
#if PG14_GTE
				bool readOnlyTree,
#endif
				ProcessUtilityContext context,
				ParamListInfo params,
				QueryEnvironment *queryEnv,
				DestReceiver *dest,
#if PG13_GTE
				QueryCompletion *completionTag
#else
				char *completionTag
#endif
);

static void
comfirm_reserved_roles(const char *target);

static void
comfirm_reserved_memberships(const char *target);

static bool
reserved_roles_check_hook(char **newval, void **extra, GucSource source);

static bool
reserved_memberships_check_hook(char **newval, void **extra, GucSource source);

static void check_parameter(char *val, char *name);


/*
 * IO: module load callback
 */
void
_PG_init(void)
{
	// Store the previous hook
	prev_hook = ProcessUtility_hook;

	// Set our hook
	ProcessUtility_hook = supautils_hook;

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
supautils_hook(PlannedStmt *pstmt,
				const char *queryString,
#if PG14_GTE
				bool readOnlyTree,
#endif
				ProcessUtilityContext context,
				ParamListInfo params,
				QueryEnvironment *queryEnv,
				DestReceiver *dest,
#if PG13_GTE
				QueryCompletion *completionTag
#else
				char *completionTag
#endif
)
{
	// Get the utility statement from the planned statement
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
					
					/* CREATE ROLE <reserved_role> */
					comfirm_reserved_roles(created_role);

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

		default:
			break;
	}

	// Chain to previously defined hooks
	if (prev_hook)
		prev_hook(pstmt, queryString,
#if PG14_GTE
				  readOnlyTree,
#endif
				  context, params, queryEnv,
				  dest, completionTag);
	else
		standard_ProcessUtility(pstmt, queryString,
#if PG14_GTE
								readOnlyTree,
#endif
								context, params, queryEnv,
								dest, completionTag);
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

static void
comfirm_reserved_memberships(const char *target)
{
	List *reserved_memberships_list;
	ListCell *membership;

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
