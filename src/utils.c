#include "pg_prelude.h"

#include "utils.h"

static bool strstarts(const char *str, const char *prefix) {
  return strncmp(str, prefix, strlen(prefix)) == 0;
}

Oid superuser_oid(const char *superuser) {
  if (superuser != NULL) {
    return get_role_oid(superuser, false);
  }
  return BOOTSTRAP_SUPERUSERID;
}

static bool role_oid_is_privileged(Oid role_oid, const char *privileged_role) {
  Oid privileged_role_oid;

  if (privileged_role == NULL || !OidIsValid(role_oid)) {
    return false;
  }
  privileged_role_oid = get_role_oid(privileged_role, true);

  return OidIsValid(privileged_role_oid) &&
         has_privs_of_role(role_oid, privileged_role_oid);
}

bool is_current_role_privileged(const char *privileged_role) {
  return role_oid_is_privileged(GetUserId(), privileged_role);
}

bool is_role_privileged(const char *role, const char *privileged_role) {
  return role_oid_is_privileged(get_role_oid(role, true), privileged_role);
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

// Sets the owner of a catalog object without checking that the new owner is a
// superuser.
//
// AlterForeignDataWrapperOwner() and AlterEventTriggerOwner() only accept a
// superuser as the new owner. AlterObjectOwner_internal() is the generic owner
// change behind most ALTER ... OWNER TO commands: it updates the owner, the
// ACL and the owner dependency as those two do, but has no such check, and it
// skips its own permission checks for a superuser, which the caller is while
// elevated.
static void set_owner(Oid class_id, Oid object_oid, Oid role_oid) {
#if PG17_GTE
  AlterObjectOwner_internal(class_id, object_oid, role_oid);
#else
  Relation catalog = table_open(class_id, RowExclusiveLock);

  AlterObjectOwner_internal(catalog, object_oid, role_oid);
  table_close(catalog, RowExclusiveLock);
#endif
  CommandCounterIncrement();
}

// Changes the OWNER of a database object.
// Postgres only lets superusers own some objects (foreign data wrappers and
// event triggers). Those are given to the role with set_owner(), which leaves
// the role itself alone: making it a superuser for the change would update its
// pg_authid row, which every database in the cluster shares.
void alter_owner(const char *obj_name, Oid role_oid,
                 altered_obj_type obj_type) {
  switch (obj_type) {
  case ALT_FDW:
    set_owner(ForeignDataWrapperRelationId,
              get_foreign_data_wrapper_oid(obj_name, false), role_oid);
    break;

  case ALT_PUB:

    AlterPublicationOwner(obj_name, role_oid);
    CommandCounterIncrement();

    break;

  case ALT_EVTRIG:
    set_owner(EventTriggerRelationId, get_event_trigger_oid(obj_name, false),
              role_oid);
    break;
  }
}

bool is_table_in_grant_list(char *const *table_names, size_t total_tables,
                            Oid target_table_id) {
  for (size_t i = 0; i < total_tables; i++) {
    List     *qual_name_list;
    RangeVar *range_var;
    Oid       table_id;

#if PG16_GTE
    qual_name_list = stringToQualifiedNameList(table_names[i], NULL);
#else
    qual_name_list = stringToQualifiedNameList(table_names[i]);
#endif
    if (qual_name_list == NULL) {
      continue;
    }

    range_var = makeRangeVarFromNameList(qual_name_list);
    // we only compare the oid against target_table_id, which the caller has
    // already locked, so there's no need to lock it again here
    table_id = RangeVarGetRelid(range_var, NoLock, true);

    if (OidIsValid(table_id) && table_id == target_table_id) {
      return true;
    }
  }

  return false;
}

#if PG17_LT
// Polyfill for pg < 17
// https://github.com/postgres/postgres/blob/3c4e26a62c31ebe296e3aedb13ac51a7a35103bd/src/common/stringinfo.c#L402-L416
void destroyStringInfo(StringInfo str) {
  Assert(str->maxlen != 0);
  pfree(str->data);
  pfree(str);
}
#endif
