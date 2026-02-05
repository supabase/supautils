#ifndef PERMISSION_HINTS_H
#define PERMISSION_HINTS_H

#include "pg_prelude.h"

typedef struct {
  Oid     relid;
  AclMode acl;
} missing_perm;

missing_perm find_missing_perm(PlannedStmt *ps, Oid current_role_oid);
void         build_privileges_string(StringInfo buf, AclMode missing_perms);

#endif /* PERMISSION_HINTS_H */
