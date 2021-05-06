#include "postgres.h"
#include "tcop/utility.h"
#include "miscadmin.h"
#include "tcop/utility.h"
#include "utils/varlena.h"

#define PG13_GTE (PG_VERSION_NUM >= 130000)

PG_MODULE_MAGIC;

void _PG_init(void);
void _PG_fini(void);

static char *reserved_roles = NULL;

static ProcessUtility_hook_type prev_utility_hook = NULL;

static void load_params(void);

static void check_role(PlannedStmt *pstmt,
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

static
void check_role(PlannedStmt *pstmt,
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
	Node		 *parsetree = pstmt->utilityStmt;

	switch (nodeTag(parsetree))
	{

		case T_GrantRoleStmt:
		{
			GrantRoleStmt *stmt = (GrantRoleStmt *) parsetree;
			ListCell *item;

			if(stmt->is_grant){
				foreach(item, stmt->granted_roles)
				{
					AccessPriv *priv = (AccessPriv *) lfirst(item);
					char *rolename = priv->priv_name;
					bool isSuper = superuser_arg(GetUserId());

					if (strcmp(rolename, "pg_read_server_files") == 0 && !isSuper)
						ereport(ERROR,
								(errcode(ERRCODE_INSUFFICIENT_PRIVILEGE),
								 errmsg("Only superusers can GRANT \"%s\"", "pg_read_server_files")));

					if (strcmp(rolename, "pg_write_server_files") == 0 && !isSuper)
						ereport(ERROR,
								(errcode(ERRCODE_INSUFFICIENT_PRIVILEGE),
								 errmsg("Only superusers can GRANT \"%s\"", "pg_write_server_files")));

					if (strcmp(rolename, "pg_execute_server_program") == 0 && !isSuper)
						ereport(ERROR,
								(errcode(ERRCODE_INSUFFICIENT_PRIVILEGE),
								 errmsg("Only superusers can GRANT \"%s\"", "pg_execute_server_program")));
				}
			}

			break;
		}

		case T_CreateRoleStmt:
		{
			CreateRoleStmt *stmt = (CreateRoleStmt *) parsetree;
			const char *role = stmt->role;
			bool isSuper = superuser_arg(GetUserId());

			List	   *reserve_list;
			ListCell *cell;

			if(!reserved_roles)
				break;

			if (!SplitIdentifierString(pstrdup(reserved_roles), ',', &reserve_list))
			{
				ereport(ERROR,
						(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
						 errmsg("parameter \"%s\" must be a comma-separated list of identifiers",
								"supautils.reserved_roles")));
			}

			foreach(cell, reserve_list)
			{
				const char *reserved_role = (const char *) lfirst(cell);

				if (strcmp(role, reserved_role) == 0 && !isSuper)
				{
					ereport(ERROR,
							(errcode(ERRCODE_RESERVED_NAME),
							 errmsg("The \"%s\" role is reserved, only superusers can create it.",
									role)));
				}

			}

			list_free(reserve_list);

			break;
		};

		case T_AlterRoleStmt:
		{
			AlterRoleStmt *stmt = (AlterRoleStmt *) parsetree;
			RoleSpec *role = stmt->role;
			bool isSuper = superuser_arg(GetUserId());

			List	   *reserve_list;
			ListCell *cell;

			// Break immediately if the role is PUBLIC, CURRENT_USER or SESSION_USER.
			if (role->roletype != ROLESPEC_CSTRING)
				break;

			if(!reserved_roles)
				break;

			if (!SplitIdentifierString(pstrdup(reserved_roles), ',', &reserve_list))
			{
				ereport(ERROR,
						(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
						 errmsg("parameter \"%s\" must be a comma-separated list of identifiers",
								"supautils.reserved_roles")));
			}

			foreach(cell, reserve_list)
			{
				const char *reserved_role = (const char *) lfirst(cell);

				if (strcmp(role->rolename, reserved_role) == 0 && !isSuper)
				{
					ereport(ERROR,
							(errcode(ERRCODE_RESERVED_NAME),
							 errmsg("The \"%s\" role is reserved, only superusers can alter it.",
									role->rolename)));
				}

			}

			list_free(reserve_list);

			break;
		}

		case T_RenameStmt:
		{
			RenameStmt *stmt = (RenameStmt *) parsetree;
			bool isSuper = superuser_arg(GetUserId());

			List	   *reserve_list;
			ListCell *cell;

			// Break immediately if not an ALTER ROLE
			if(stmt->renameType != OBJECT_ROLE)
				break;

			if(!reserved_roles)
				break;

			if (!SplitIdentifierString(pstrdup(reserved_roles), ',', &reserve_list))
				ereport(ERROR,
						(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
						 errmsg("parameter \"%s\" must be a comma-separated list of identifiers",
								"supautils.reserved_roles")));

			foreach(cell, reserve_list)
			{
				const char *reserved_role = (const char *) lfirst(cell);

				if (strcmp(stmt->subname, reserved_role) == 0 && !isSuper)
					ereport(ERROR,
							(errcode(ERRCODE_RESERVED_NAME),
							 errmsg("The \"%s\" role is reserved, only superusers can rename it.",
									stmt->subname)));

				if (strcmp(stmt->newname, reserved_role) == 0 && !isSuper)
					ereport(ERROR,
							(errcode(ERRCODE_RESERVED_NAME),
							 errmsg("The \"%s\" role is reserved, only superusers can rename it.",
									stmt->newname)));

			}

			list_free(reserve_list);

			break;
		}

		case T_DropRoleStmt:
		{
			DropRoleStmt *stmt = (DropRoleStmt *) parsetree;
			ListCell *item;
			bool isSuper = superuser_arg(GetUserId());

			List	   *reserve_list;
			ListCell *cell;

			if(!reserved_roles)
				break;

			if (!SplitIdentifierString(pstrdup(reserved_roles), ',', &reserve_list))
			{
				ereport(ERROR,
						(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
						 errmsg("parameter \"%s\" must be a comma-separated list of identifiers",
								"supautils.reserved_roles")));
			}

			foreach(item, stmt->roles)
			{
				RoleSpec *role = lfirst(item);

				foreach(cell, reserve_list)
				{
					const char *reserved_role = (const char *) lfirst(cell);

					if (strcmp(role->rolename, reserved_role) == 0 && !isSuper)
					{
						ereport(ERROR,
								(errcode(ERRCODE_RESERVED_NAME),
								 errmsg("The \"%s\" role is reserved, only superusers can drop it",
										role->rolename)));
					}
				}
			}

			list_free(reserve_list);

			break;
		}

		default:
			break;
	}

	if (prev_utility_hook)
		(*prev_utility_hook) (pstmt, queryString,
								context, params, queryEnv,
								dest, completionTag);
	else
		standard_ProcessUtility(pstmt, queryString,
								context, params, queryEnv,
								dest, completionTag);
}

static
void load_params(void)
{
	DefineCustomStringVariable("supautils.reserved_roles",
							   "Non-superuser roles that can only be created, altered or dropped by superusers",
							   NULL,
							   &reserved_roles,
							   NULL,
							   PGC_POSTMASTER, 0,
								 NULL, NULL, NULL);
}

void
_PG_init(void)
{
	prev_utility_hook = ProcessUtility_hook;
	ProcessUtility_hook = check_role;

	load_params();
}
void
_PG_fini(void)
{
	ProcessUtility_hook = prev_utility_hook;
}
