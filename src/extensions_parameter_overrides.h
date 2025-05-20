#ifndef EXTENSIONS_PARAMETER_OVERRIDES_H
#define EXTENSIONS_PARAMETER_OVERRIDES_H

#include "pg_prelude.h"

typedef struct {
    char *name;
    char *schema;
} extension_parameter_overrides;

typedef enum {
    JEPO_EXPECT_TOPLEVEL_START,
    JEPO_EXPECT_TOPLEVEL_FIELD,
    JEPO_EXPECT_PARAMETER_OVERRIDES_START,
    JEPO_EXPECT_SCHEMA,
    JEPO_UNEXPECTED_FIELD,
    JEPO_UNEXPECTED_ARRAY,
    JEPO_UNEXPECTED_SCALAR,
    JEPO_UNEXPECTED_OBJECT,
    JEPO_UNEXPECTED_SCHEMA_VALUE
} json_extension_parameter_overrides_semantic_state;

typedef struct {
    json_extension_parameter_overrides_semantic_state state;
    char *error_msg;
    int total_epos;
    extension_parameter_overrides *epos;
} json_extension_parameter_overrides_parse_state;

extern json_extension_parameter_overrides_parse_state
parse_extensions_parameter_overrides(const char *str,
                                     extension_parameter_overrides *epos);

extern void
override_create_ext_statement(CreateExtensionStmt *stmt,
                              const size_t total_epos,
                              const extension_parameter_overrides *epos);

#endif
