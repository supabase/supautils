#ifndef DROP_TRIGGER_GRANTS_H
#define DROP_TRIGGER_GRANTS_H

#define MAX_DROP_TRIGGER_GRANT_TABLES 100

typedef struct {
    char *role_name;
    char *table_names[MAX_DROP_TRIGGER_GRANT_TABLES];
    size_t total_tables;
} drop_trigger_grants;

typedef enum {
    JDTG_EXPECT_TOPLEVEL_START,
    JDTG_EXPECT_TOPLEVEL_FIELD,
    JDTG_EXPECT_TABLES_START,
    JDTG_EXPECT_TABLE,
    JDTG_UNEXPECTED_ARRAY,
    JDTG_UNEXPECTED_SCALAR,
    JDTG_UNEXPECTED_OBJECT,
    JDTG_UNEXPECTED_TABLE_VALUE
} json_drop_trigger_grants_semantic_state;

typedef struct {
    json_drop_trigger_grants_semantic_state state;
    char *error_msg;
    int total_dtgs;
    drop_trigger_grants *dtgs;
} json_drop_trigger_grants_parse_state;

extern json_drop_trigger_grants_parse_state
parse_drop_trigger_grants(const char *str, drop_trigger_grants *dtgs);

extern bool
is_current_role_granted_table_drop_trigger(const RangeVar *table_range_var,
                                           const drop_trigger_grants *dtgs,
                                           const size_t total_dtgs);

#endif
