#include "postgres.h"
#include "tcop/utility.h"
#include "miscadmin.h"

#define PG13_GTE (PG_VERSION_NUM >= 130000)

PG_MODULE_MAGIC;

void _PG_init(void);
void _PG_fini(void);

static ProcessUtility_hook_type prev_utility_hook = NULL;

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

		case T_AlterRoleStmt:
		{
			AlterRoleStmt *stmt = (AlterRoleStmt *) parsetree;
			RoleSpec *role = stmt->role;
			bool isSuper = superuser_arg(GetUserId());

			// Return immediately if the role is PUBLIC, CURRENT_USER or SESSION_USER.
			if (role->roletype != ROLESPEC_CSTRING)
				return;

			if (strcmp(role->rolename, "anon") == 0 && !isSuper)
			{
				ereport(ERROR,
						(errcode(ERRCODE_RESERVED_NAME),
						 errmsg("The \"%s\" role is reserved by Supabase, only superusers can alter it.",
								role->rolename)));
			}

		}

	case T_DropRoleStmt:
		{
			DropRoleStmt *stmt = (DropRoleStmt *) parsetree;
			ListCell *item;

			bool isSuper = superuser_arg(GetUserId());

			foreach(item, stmt->roles)
			{
				RoleSpec *role = lfirst(item);

				if (strcmp(role->rolename, "anon") == 0 && !isSuper)
				{
					ereport(ERROR,
							(errcode(ERRCODE_RESERVED_NAME),
							 errmsg("The \"%s\" role is reserved by Supabase, only superusers can drop it",
									role->rolename)));
				}

			}

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

void
_PG_init(void)
{
	prev_utility_hook = ProcessUtility_hook;
	ProcessUtility_hook = check_role;
}
void
_PG_fini(void)
{
	ProcessUtility_hook = prev_utility_hook;
}
