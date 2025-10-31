#ifndef POLICY_GRANTS_H
#define POLICY_GRANTS_H

#include <postgres.h>

#include <catalog/namespace.h>

#define MAX_POLICY_GRANT_TABLES 100

typedef struct {
  char  *role_name;
  char  *table_names[MAX_POLICY_GRANT_TABLES];
  size_t total_tables;
} policy_grants;

typedef enum {
  JPG_EXPECT_TOPLEVEL_START,
  JPG_EXPECT_TOPLEVEL_FIELD,
  JPG_EXPECT_TABLES_START,
  JPG_EXPECT_TABLE,
  JPG_UNEXPECTED_ARRAY,
  JPG_UNEXPECTED_SCALAR,
  JPG_UNEXPECTED_OBJECT,
  JPG_UNEXPECTED_TABLE_VALUE
} json_policy_grants_semantic_state;

typedef struct {
  json_policy_grants_semantic_state state;
  char                             *error_msg;
  int                               total_pgs;
  policy_grants                    *pgs;
} json_policy_grants_parse_state;

extern json_policy_grants_parse_state parse_policy_grants(const char    *str,
                                                          policy_grants *pgs);

extern bool
is_current_role_granted_table_policy(const RangeVar      *table_range_var,
                                     const policy_grants *pgs,
                                     const size_t         total_pgs);

#endif
