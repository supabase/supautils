#include "pg_prelude.h"
#include "utils.h"

static Oid prev_role_oid = 0;
static int prev_role_sec_context = 0;

// Prevent nested switch_to_superuser() calls from corrupting prev_role_*
static bool is_switched_to_superuser = false;

static bool strstarts(const char *str, const char *prefix) {
    return strncmp(str, prefix, strlen(prefix)) == 0;
}

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

// Changes the OWNER of a database object.
// Some objects (e.g. foreign data wrappers) can only be owned by superusers, so this switches to superuser accordingly and then goes backs to non-super.
void alter_owner(const char *obj_name, const char *role_name, altered_obj_type obj_type){

  // static allocations to avoid dynamic palloc
  static const char sql_fdw_template[] =
    "alter role \"%s\" superuser;\n"
    "alter foreign data wrapper \"%s\" owner to \"%s\";\n"
    "alter role \"%s\" nosuperuser;\n";

  static const char sql_evtrig_template[] =
    "alter role \"%s\" superuser;\n"
    "alter event trigger \"%s\" owner to \"%s\";\n"
    "alter role \"%s\" nosuperuser;\n";

  static const char sql_sub_template[] =
    "alter publication \"%s\" owner to \"%s\";\n";

  // max sql length for above templates
  static const size_t max_sql_len
        = sizeof (sql_fdw_template) // the fdw template is the largest one
        + 4 * NAMEDATALEN; // the max size of the 4 identifiers on the fdw template

  char sql[max_sql_len];

  switch(obj_type){
  case ALT_FDW:
    snprintf(sql,
             max_sql_len,
             sql_fdw_template,
             role_name,
             obj_name,
             role_name,
             role_name);
    break;
  case ALT_PUB:
    snprintf(sql,
             max_sql_len,
             sql_sub_template,
             obj_name,
             role_name);
    break;
  case ALT_EVTRIG:
    snprintf(sql,
             max_sql_len,
             sql_evtrig_template,
             role_name,
             obj_name,
             role_name,
             role_name);
    break;
  }

  PushActiveSnapshot(GetTransactionSnapshot());
  SPI_connect();

  int rc = SPI_execute(sql, false, 0);
  if (rc != SPI_OK_UTILITY) {
      elog(ERROR, "SPI_execute failed with error code %d", rc);
  }

  SPI_finish();
  PopActiveSnapshot();
}
