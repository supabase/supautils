#include "postgres.h"
#include "tcop/utility.h"

#define PG13_GTE (PG_VERSION_NUM >= 130000)

PG_MODULE_MAGIC;

void _PG_init(void);
void _PG_fini(void);

static ProcessUtility_hook_type prev_utility_hook = NULL;

static void check_role_membership(PlannedStmt *pstmt,
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
void check_role_membership(PlannedStmt *pstmt,
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

					if (strcmp(rolename, "pg_read_server_files") == 0)
						ereport(ERROR,
								(errcode(ERRCODE_INSUFFICIENT_PRIVILEGE),
								 errmsg("Cannot GRANT \"%s\"", "pg_read_server_files")));

					if (strcmp(rolename, "pg_execute_server_program") == 0)
						ereport(ERROR,
								(errcode(ERRCODE_INSUFFICIENT_PRIVILEGE),
								 errmsg("Cannot GRANT \"%s\"", "pg_execute_server_program")));
				}
			}

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

void
_PG_init(void)
{
	prev_utility_hook = ProcessUtility_hook;
	ProcessUtility_hook = check_role_membership;
}
void
_PG_fini(void)
{
	ProcessUtility_hook = prev_utility_hook;
}
