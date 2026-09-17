#ifndef EXTENSIONS_H
#define EXTENSIONS_H

#include "pg_prelude.h"

#include "constrained_extensions.h"
#include "extensions_parameter_overrides.h"

typedef enum {
  RESTRICT_EXTENSION_VERSIONS_OFF,
  RESTRICT_EXTENSION_VERSIONS_WARN,
  RESTRICT_EXTENSION_VERSIONS_ERROR
} restrict_extension_versions_mode;

// What the extension handlers need from the configuration.
typedef struct {
  const char                          *superuser;
  const char                          *privileged_role;
  const char                          *privileged_extensions;
  const char                          *custom_scripts_path;
  constrained_extension               *constrained;
  size_t                               total_constrained;
  const extension_parameter_overrides *overrides;
  size_t                               total_overrides;
  restrict_extension_versions_mode     restrict_versions;
} extension_policy;

/**
 * Handles the extension DDL that supautils intercepts: CREATE, ALTER, ALTER
 * SET SCHEMA, DROP and COMMENT ON EXTENSION.
 *
 * Returns true when the statement was run, i.e. the previous hook has been
 * called and the caller must not call it again. Returns false when the hook
 * should carry on with its normal processing; the statement's options may
 * have been rewritten in that case too.
 */
extern bool handle_extension_stmt(Node *stmt, const utility_hook_args *args,
                                  const extension_policy *policy);

#endif
