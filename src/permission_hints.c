#include "pg_prelude.h"

#include "permission_hints.h"

// builds a comma-separated list of missing privileges
void build_privileges_string(StringInfo buf, AclMode missing_acl) {
  static const struct {
    AclMode     acl;
    const char *name;
  } privilege_map[] = {{ACL_SELECT, "SELECT"},
                       {ACL_INSERT, "INSERT"},
                       {ACL_UPDATE, "UPDATE"},
                       {ACL_DELETE, "DELETE"}};

  for (size_t i = 0; i < lengthof(privilege_map); i++) {
    if (missing_acl & privilege_map[i].acl) {
      if (buf->len > 0) appendStringInfoString(buf, ", ");
      appendStringInfoString(buf, privilege_map[i].name);
    }
  }
}

// This logic is mostly copied from ExecCheckPermissionsModified in core, see
// https://github.com/postgres/postgres/blob/851f6649cc18c4b482fa2b6afddb65b35d035370/src/backend/executor/execMain.c#L755
// With the difference that we also include SELECT column privileges handling
// for consolidation
static bool has_column_perms(Oid relid, Oid userid, Bitmapset *cols,
                             AclMode required_perms) {
  int col = -1;

  // If no column is selected (e.g. SELECT count(*) from table) allow SELECT if
  // there's any column-level priv On INSERT this also happens on INSERT DEFAULT
  // VALUES and on UPDATE it also happens on SELECT FOR UPDATE
  if (!cols || bms_is_empty(cols)) {
    return pg_attribute_aclcheck_all(relid, userid, required_perms,
                                     ACLMASK_ANY) == ACLCHECK_OK;
  }

  // otherwise check for all column privileges
  while ((col = bms_next_member(cols, col)) >= 0) {
    AttrNumber attno = col + FirstLowInvalidHeapAttributeNumber;

    // This handles the case of `SELECT *` (`*` is InvalidAttrNumber), so we
    // have to check for all column privs
    // `*` cannot happen for INSERT and UDPATE so we don't handle them
    if (attno == InvalidAttrNumber && required_perms == ACL_SELECT) {
      if (pg_attribute_aclcheck_all(relid, userid, required_perms,
                                    ACLMASK_ALL) != ACLCHECK_OK)
        return false;
    } else { // check all columns
      if (pg_attribute_aclcheck(relid, attno, userid, required_perms) !=
          ACLCHECK_OK)
        return false;
    }
  }

  return true;
}

/*
 * The logic to get the missing privileges is mostly copied from
 * ExecCheckOneRelPerms from core, see
 * https://github.com/postgres/postgres/blob/851f6649cc18c4b482fa2b6afddb65b35d035370/src/backend/executor/execMain.c#L646.
 *
 * In addition to table-level privilees, we have to check columns privileges too
 * because on postgres while `GRANT SELECT ON TABLE` has the same effect as
 * `GRANT SELECT(on,every,col) ON TABLE` both cases are stored differently
 * on the Acl structs. GRANTing on every column doesn't set the table-wide
 * SELECT.
 *
 * DELETE has no column-level privileges, so there's column-level logic for
 * ACL_DELETE
 */
static AclMode get_missing_aclmode(
#if PG16_LT
    const RangeTblEntry *info,
#else
    const RTEPermissionInfo *info,
#endif
    Oid current_role_oid) {

  Oid userid =
      OidIsValid(info->checkAsUser) ? info->checkAsUser : current_role_oid;

  // check for all the table-level privs the current role has
  AclMode rel_perm =
      pg_class_aclmask(info->relid, userid, info->requiredPerms, ACLMASK_ALL);

  // intersect required with missing table-level perms
  AclMode missing_perms = info->requiredPerms & ~rel_perm;

  // Is the table-wide SELECT privilege missing? If so, check for complete
  // column-level privileges
  if ((missing_perms & ACL_SELECT) &&
      has_column_perms(info->relid, userid, info->selectedCols, ACL_SELECT))
    missing_perms &=
        ~ACL_SELECT; // if all good then remove ACL_SELECT from missing_perms

  // Same for INSERT and UPDATE
  if ((missing_perms & ACL_INSERT) &&
      has_column_perms(info->relid, userid, info->insertedCols, ACL_INSERT))
    missing_perms &= ~ACL_INSERT;

  if ((missing_perms & ACL_UPDATE) &&
      has_column_perms(info->relid, userid, info->updatedCols, ACL_UPDATE))
    missing_perms &= ~ACL_UPDATE;

  return missing_perms;
}

// Given the required RTEPermissionInfo list from the query and the role oid,
// we find the first missing ACL (which should also match the failed acl on
// ERRCODE_INSUFFICIENT_PRIVILEGE)
missing_perm find_missing_perm(PlannedStmt *ps, Oid current_role_oid) {
  missing_perm result = {.relid = InvalidOid, .acl = 0};

#if PG16_LT
  foreach_ptr(RangeTblEntry, info, ps->rtable) {
    // on these versions, views contain an invalid oid on the first entry
    if (!OidIsValid(info->relid)) continue;
#else
  foreach_ptr(RTEPermissionInfo, info, ps->permInfos) {
#endif
    AclMode missing_acl = get_missing_aclmode(info, current_role_oid);

    if (missing_acl != 0) {
      result.relid = info->relid;
      result.acl   = missing_acl;
      return result;
    }
  }

  return result;
}
