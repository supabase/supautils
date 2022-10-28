#include <postgres.h>

#include <utils/varlena.h>

#include "utils.h"

void alter_role_with_bypassrls_option_as_superuser(const char *role_name,
                                                   DefElem *bypassrls_option,
                                                   const char *superuser_name) {
    Oid superuser_oid = BOOTSTRAP_SUPERUSERID;
    Oid prev_role_oid = 0;
    int prev_role_sec_context = 0;

    RoleSpec *role = makeNode(RoleSpec);
    AlterRoleStmt *bypassrls_stmt = makeNode(AlterRoleStmt);

    if (superuser_name != NULL) {
        superuser_oid = get_role_oid(superuser_name, false);
    }

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

bool is_string_in_comma_delimited_string(const char *s1, char *s2) {
    bool s1_is_in_s2 = false;
    List *split_s2 = NIL;
    ListCell *lc;

    SplitIdentifierString(s2, ',', &split_s2);

    foreach (lc, split_s2) {
        char *s2_elem = (char *)lfirst(lc);

        if (strcmp(s1, s2_elem) == 0) {
            s1_is_in_s2 = true;
            break;
        }
    }
    list_free(split_s2);

    return s1_is_in_s2;
}
