#include <postgres.h>

#include <catalog/namespace.h>
#include <utils/regproc.h>
#include <utils/varlena.h>

#include "utils.h"

static Oid prev_role_oid = 0;
static int prev_role_sec_context = 0;

// Prevent nested switch_to_superuser() calls from corrupting prev_role_*
static bool is_switched_to_superuser = false;

static bool strstarts(const char *, const char *);

void switch_to_superuser(const char *supauser,
                         bool *already_switched) {
    Oid superuser_oid = BOOTSTRAP_SUPERUSERID;
    *already_switched = is_switched_to_superuser;

    if (*already_switched) {
        return;
    }
    is_switched_to_superuser = true;

    if (supauser != NULL) {
        superuser_oid = get_role_oid(supauser, false);
    }

    GetUserIdAndSecContext(&prev_role_oid, &prev_role_sec_context);
    SetUserIdAndSecContext(superuser_oid, prev_role_sec_context |
                                              SECURITY_LOCAL_USERID_CHANGE |
                                              SECURITY_RESTRICTED_OPERATION);
}

void switch_to_original_role(void) {
    SetUserIdAndSecContext(prev_role_oid, prev_role_sec_context);
    is_switched_to_superuser = false;
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

        if ((remove_ending_wildcard(s2_elem) && strstarts(s1, s2_elem)) ||
            strcmp(s1, s2_elem) == 0) {
            s1_is_in_s2 = true;
            break;
        }
    }
    list_free(split_s2);

    pfree(s2_tmp);

    return s1_is_in_s2;
}

bool remove_ending_wildcard(char *elem) {
    bool wildcard_removed = false;
    if (elem) {
        size_t elem_size = strlen(elem);

        if (elem_size > 1 && elem[elem_size - 1] == '*') {
            wildcard_removed = true;
            elem[elem_size - 1] =
                '\0'; // remove the '*' from the end of the string
        }
    }

    return wildcard_removed;
}

bool strstarts(const char *str, const char *prefix) {
    return strncmp(str, prefix, strlen(prefix)) == 0;
}
