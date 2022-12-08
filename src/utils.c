#include <postgres.h>

#include <utils/varlena.h>

#include "utils.h"

static Oid prev_role_oid = 0;
static int prev_role_sec_context = 0;

void alter_role_with_bypassrls_option_as_superuser(const char *role_name,
                                                   DefElem *bypassrls_option,
                                                   const char *superuser_name) {
    RoleSpec *role = makeNode(RoleSpec);
    AlterRoleStmt *bypassrls_stmt = makeNode(AlterRoleStmt);

    role->roletype = ROLESPEC_CSTRING;
    role->rolename = pstrdup(role_name);
    role->location = -1;

    bypassrls_stmt->role = role;
    bypassrls_stmt->options = list_make1(bypassrls_option);

    switch_to_superuser(superuser_name);

#if PG15_GTE
    AlterRole(NULL, bypassrls_stmt);
#else
    AlterRole(bypassrls_stmt);
#endif

    switch_to_original_role();

    pfree(role->rolename);
    pfree(role);
    list_free(bypassrls_stmt->options);
    pfree(bypassrls_stmt);

    return;
}

void switch_to_superuser(const char *privileged_extensions_superuser) {
    Oid superuser_oid = BOOTSTRAP_SUPERUSERID;

    if (privileged_extensions_superuser != NULL) {
        superuser_oid = get_role_oid(privileged_extensions_superuser, false);
    }

    GetUserIdAndSecContext(&prev_role_oid, &prev_role_sec_context);
    SetUserIdAndSecContext(superuser_oid, prev_role_sec_context |
                                              SECURITY_LOCAL_USERID_CHANGE |
                                              SECURITY_RESTRICTED_OPERATION);
}

void switch_to_original_role(void) {
    SetUserIdAndSecContext(prev_role_oid, prev_role_sec_context);
}

bool is_string_in_comma_delimited_string(const char *s1, const char *s2) {
    bool s1_is_in_s2 = false;
    char *s2_tmp;
    List *split_s2 = NIL;
    ListCell *lc;

    if (s1 == NULL || s2 == NULL) {
        return false;
    }

    s2_tmp = pstrdup(s2);

    SplitIdentifierString(s2_tmp, ',', &split_s2);

    foreach (lc, split_s2) {
        char *s2_elem = (char *)lfirst(lc);

        if (strcmp(s1, s2_elem) == 0) {
            s1_is_in_s2 = true;
            break;
        }
    }
    list_free(split_s2);

    pfree(s2_tmp);

    return s1_is_in_s2;
}
