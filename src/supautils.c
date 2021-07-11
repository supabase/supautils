#include "postgres.h"
#include "tcop/utility.h"
#include "miscadmin.h"
#include "utils/varlena.h"
#include "utils/acl.h"

#define PG13_GTE (PG_VERSION_NUM >= 130000)

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

static char* look_for_reserved_membership(Node *utility_stmt,
							List *memberships_list);
static char* look_for_reserved_role(Node *utility_stmt,
							List *roles_list);

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
								NULL, NULL, NULL);

	DefineCustomStringVariable("supautils.reserved_memberships",
								"Comma-separated list of roles whose memberships cannot be granted",
								NULL,
								&reserved_memberships,
								NULL,
								PGC_SIGHUP, 0,
								NULL, NULL, NULL);
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

	// Check reserved objects if not a superuser
	if (!superuser())
	{

		// Check if supautils.reserved_memberships is not empty
		if(reserved_memberships)
		{
			List *memberships_list;
			char *reserved_membership = NULL;

			// split the comma-separated string into a List by using a
			// helper function from varlena.h
			if (!SplitIdentifierString(pstrdup(reserved_memberships), ',', &memberships_list))
				// abort and report an error if the splitting fails
				EREPORT_INVALID_PARAMETER("supautils.reserved_memberships");

			// Do the core logic
			reserved_membership = look_for_reserved_membership(utility_stmt, memberships_list);

			// Need to free the list according to the SplitIdentifierString implementation,
			// defined in src/backend/utils/adt/varlena.c
			list_free(memberships_list);

			// Fail if there's a reserved membership in the statement
			if(reserved_membership)
				EREPORT_RESERVED_MEMBERSHIP(reserved_membership);
		}

		// Ditto for supautils.reserved_roles
		if(reserved_roles)
		{
			List *roles_list;
			char *reserved_role = NULL;

			if (!SplitIdentifierString(pstrdup(reserved_roles), ',', &roles_list))
				EREPORT_INVALID_PARAMETER("supautils.reserved_roles");

			reserved_role = look_for_reserved_role(utility_stmt, roles_list);

			list_free(roles_list);

			if(reserved_role)
				EREPORT_RESERVED_ROLE(reserved_role);
		}
	}

	// Chain to previously defined hooks
	if (prev_hook)
		prev_hook(pstmt, queryString,
								context, params, queryEnv,
								dest, completionTag);
	else
		standard_ProcessUtility(pstmt, queryString,
								context, params, queryEnv,
								dest, completionTag);
}

/*
 * Look if the utility statement grants a reserved membership,
 * return the membership if it does
 */
static char*
look_for_reserved_membership(Node *utility_stmt, List *memberships_list)
{
	// Check the utility statement type
	switch (utility_stmt->type)
	{
		//GRANT <role> TO <another_role>
		case T_GrantRoleStmt:
			{
				GrantRoleStmt *stmt = (GrantRoleStmt *) utility_stmt;
				ListCell *role_cell;

				if(stmt->is_grant)
				{
					foreach(role_cell, stmt->granted_roles)
					{
						AccessPriv *priv = (AccessPriv *) lfirst(role_cell);
						ListCell *membership_cell;
						const char *rolename = priv->priv_name;

						foreach(membership_cell, memberships_list)
						{
							char *reserved_membership = (char *) lfirst(membership_cell);

							if (strcmp(rolename, reserved_membership) == 0)
								return reserved_membership;
						}
					}
				}

				break;
			}
		// CREATE ROLE <any_role> has ways to add memberships
		case T_CreateRoleStmt:
			{
				CreateRoleStmt *stmt = (CreateRoleStmt *) utility_stmt;

				const char *role = stmt->role;
				ListCell   *option_cell;

				List *addroleto = NIL;	/* roles to make this a member of */
				bool hasrolemembers = false;	/* has roles to be members of this role */

				foreach(option_cell, stmt->options)
				{
					DefElem   *defel = (DefElem *) lfirst(option_cell);
					if (strcmp(defel->defname, "addroleto") == 0)
					{
						addroleto = (List *) defel->arg;
					}

					if (strcmp(defel->defname, "rolemembers") == 0 || strcmp(defel->defname, "adminmembers") == 0)
					{
						hasrolemembers = true;
					}
				}

				// CREATE ROLE <any_role> IN ROLE/GROUP <role_with_reserved_membership>
				if (addroleto)
				{
					ListCell   *role_cell;

					foreach(role_cell, addroleto)
					{
						RoleSpec *rolemember = lfirst(role_cell);
						ListCell *membership_cell;

						foreach(membership_cell, memberships_list)
						{
							char *reserved_membership = (char *) lfirst(membership_cell);

							if (strcmp(get_rolespec_name(rolemember), reserved_membership) == 0)
								return reserved_membership;
						}
					}
				}

				// CREATE ROLE <role_with_reserved_membership> ROLE/ADMIN/USER <any_role>
				// This is a contrived case because the "role_with_reserved_membership" should already exist, but handle it anyway.
				if (hasrolemembers)
				{
					ListCell *membership_cell;

					foreach(membership_cell, memberships_list)
					{
						char *reserved_membership = (char *) lfirst(membership_cell);

						if (strcmp(role, reserved_membership) == 0)
							return reserved_membership;
					}
				}

				break;
			}
		default:
			break;
	}

	return NULL;
}

/*
 * Look if the utility statement modifies a reserved role,
 * return the role if it does
 */
static char*
look_for_reserved_role(Node *utility_stmt, List *roles_list)
{
	switch (nodeTag(utility_stmt))
	{
		//CREATE ROLE <role>
		case T_CreateRoleStmt:
			{
				CreateRoleStmt *stmt = (CreateRoleStmt *) utility_stmt;
				ListCell *role_cell;

				const char *role = stmt->role;

				// if role already exists, bypass the hook to let it fail with the usual error
				if (OidIsValid(get_role_oid(role, true)))
					break;

				foreach(role_cell, roles_list)
				{
					char *reserved_role = (char *) lfirst(role_cell);

					if (strcmp(role, reserved_role) == 0)
						return reserved_role;
				}

				break;
			}
		// ALTER ROLE <role> NOLOGIN NOINHERIT..
		case T_AlterRoleStmt:
			{
				AlterRoleStmt *stmt = (AlterRoleStmt *) utility_stmt;
				RoleSpec *role = stmt->role;
				ListCell *role_cell;

				foreach(role_cell, roles_list)
				{
					char *reserved_role = (char *) lfirst(role_cell);

					if (strcmp(get_rolespec_name(role), reserved_role) == 0)
						return reserved_role;
				}

				break;
			}
		// ALTER ROLE <role> SET search_path TO ...
		case T_AlterRoleSetStmt:
			{
				AlterRoleSetStmt *stmt = (AlterRoleSetStmt *) utility_stmt;
				RoleSpec *role = stmt->role;
				ListCell *role_cell;

				foreach(role_cell, roles_list)
				{
					char *reserved_role = (char *) lfirst(role_cell);

					if (strcmp(get_rolespec_name(role), reserved_role) == 0)
						return reserved_role;
				}

				break;
			}
		// ALTER ROLE <role> RENAME TO ...
		case T_RenameStmt:
			{
				RenameStmt *stmt = (RenameStmt *) utility_stmt;
				ListCell *role_cell;

				foreach(role_cell, roles_list)
				{
					char *reserved_role = (char *) lfirst(role_cell);

					if (strcmp(stmt->subname, reserved_role) == 0 ||
							strcmp(stmt->newname, reserved_role) == 0)
						return reserved_role;
				}

				break;
			}
		// DROP ROLE <role>
		case T_DropRoleStmt:
			{
				DropRoleStmt *stmt = (DropRoleStmt *) utility_stmt;
				ListCell *item;

				foreach(item, stmt->roles)
				{
					RoleSpec *role = lfirst(item);
					ListCell *role_cell;

					// Break if the role is PUBLIC, let pg give a better error later
					if (role->roletype == ROLESPEC_PUBLIC)
						break;

					foreach(role_cell, roles_list)
					{
						char *reserved_role = (char *) lfirst(role_cell);

						if (strcmp(get_rolespec_name(role), reserved_role) == 0)
							return reserved_role;
					}
				}

				break;
			}
		default:
			break;
	}
	return NULL;
}