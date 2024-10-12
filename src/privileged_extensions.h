#ifndef PRIVILEGED_EXTENSIONS_H
#define PRIVILEGED_EXTENSIONS_H

#include "extensions_parameter_overrides.h"
#include "utils.h"

extern void handle_create_extension(
    void (*process_utility_hook)(PROCESS_UTILITY_PARAMS),
    PROCESS_UTILITY_PARAMS, const char *privileged_extensions,
    const char *privileged_extensions_superuser,
    const char *privileged_extensions_custom_scripts_path,
    const extension_parameter_overrides *epos, const size_t total_epos);

extern void
handle_alter_extension(void (*process_utility_hook)(PROCESS_UTILITY_PARAMS),
                       PROCESS_UTILITY_PARAMS,
                       const char *extname,
                       const char *privileged_extensions,
                       const char *privileged_extensions_superuser);

extern void
handle_drop_extension(void (*process_utility_hook)(PROCESS_UTILITY_PARAMS),
                      PROCESS_UTILITY_PARAMS, const char *privileged_extensions,
                      const char *privileged_extensions_superuser);

#endif
