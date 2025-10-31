#include "pg_prelude.h"

#include "utils.h"

static Oid prev_role_oid         = 0;
static int prev_role_sec_context = 0;

// Prevent nested switch_to_superuser() calls from corrupting prev_role_*
static bool is_switched_to_superuser = false;

static bool strstarts(const char *str, const char *prefix) {
  return strncmp(str, prefix, strlen(prefix)) == 0;
}

void switch_to_superuser(const char *supauser, bool *already_switched) {
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
  bool      s1_is_in_s2 = false;
  char     *s2_tmp;
  List     *split_s2 = NIL;
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
      wildcard_removed    = true;
      elem[elem_size - 1] = '\0'; // remove the '*' from the end of the string
    }
  }

  return wildcard_removed;
}

static void alter_role_super(const char *rolename, bool make_super) {
  RoleSpec *rolespec = makeNode(RoleSpec);
  rolespec->roletype = ROLESPEC_CSTRING;
  rolespec->rolename = pstrdup(rolename);
  rolespec->location = -1;

  AlterRoleStmt *alter_stmt = makeNode(AlterRoleStmt);
  alter_stmt->role          = rolespec;

#if PG15_GTE
  alter_stmt->options =
      list_make1(makeDefElem("superuser", (Node *)makeBoolean(make_super), -1));

  AlterRole(NULL, alter_stmt);
#else
  alter_stmt->options =
      list_make1(makeDefElem("superuser", (Node *)makeInteger(make_super), -1));

  AlterRole(alter_stmt);
#endif

  CommandCounterIncrement();
}

// Changes the OWNER of a database object.
// Some objects (e.g. foreign data wrappers) can only be owned by superusers, so
// this switches to superuser accordingly and then goes backs to non-super.
void alter_owner(const char *obj_name, Oid role_oid,
                 altered_obj_type obj_type) {
  switch (obj_type) {
  case ALT_FDW: {
    char *role_name = GetUserNameFromId(role_oid, false);
    alter_role_super(role_name, true);

    AlterForeignDataWrapperOwner(obj_name, role_oid);
    CommandCounterIncrement();

    alter_role_super(role_name, false);

    break;
  }

  case ALT_PUB:

    AlterPublicationOwner(obj_name, role_oid);
    CommandCounterIncrement();

    break;

  case ALT_EVTRIG: {
    char *role_name = GetUserNameFromId(role_oid, false);

    alter_role_super(role_name, true);

    AlterEventTriggerOwner(obj_name, role_oid);
    CommandCounterIncrement();

    alter_role_super(role_name, false);

    break;
  }
  }
}
