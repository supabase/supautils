#ifndef PERMISSION_HINTS_H
#define PERMISSION_HINTS_H

#include "pg_prelude.h"

typedef struct {
  Oid     relid;
  AclMode acl;
} missing_perm;

#if PG16_LT
missing_perm find_missing_perm(List *rtable, Oid current_role_oid);
#else
missing_perm find_missing_perm(List *perminfos, Oid current_role_oid);
#endif
void         build_privileges_string(StringInfo buf, AclMode missing_perms);

#endif /* PERMISSION_HINTS_H */
