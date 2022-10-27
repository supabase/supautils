#include "utils.h"

void alter_role_with_bypassrls_option_as_superuser(const char *role_name,
                                                   DefElem *bypassrls_option) {
    Oid superuser_oid = BOOTSTRAP_SUPERUSERID;
    Oid prev_role_oid = 0;
    int prev_role_sec_context = 0;

    RoleSpec *role = makeNode(RoleSpec);
    AlterRoleStmt *bypassrls_stmt = makeNode(AlterRoleStmt);

    role->roletype = ROLESPEC_CSTRING;
    role->rolename = pstrdup(role_name);
    role->location = -1;

    bypassrls_stmt->role = role;
    bypassrls_stmt->options = list_make1(bypassrls_option);

    GetUserIdAndSecContext(&prev_role_oid, &prev_role_sec_context);
    SetUserIdAndSecContext(superuser_oid, prev_role_sec_context |
                                              SECURITY_LOCAL_USERID_CHANGE |
                                              SECURITY_RESTRICTED_OPERATION);

#if PG15_GTE
    AlterRole(NULL, bypassrls_stmt);
#else
    AlterRole(bypassrls_stmt);
#endif

    SetUserIdAndSecContext(prev_role_oid, prev_role_sec_context);

    pfree(role->rolename);
    pfree(role);
    list_free(bypassrls_stmt->options);
    pfree(bypassrls_stmt);

    return;
}
