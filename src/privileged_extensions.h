#ifndef PRIVILEGED_EXTENSIONS_H
#define PRIVILEGED_EXTENSIONS_H

#include "utils.h"

extern void
handle_create_extension(void (*process_utility_hook)(PROCESS_UTILITY_PARAMS),
                        PROCESS_UTILITY_PARAMS, char *privileged_extensions,
                        char *privileged_extensions_superuser,
                        char *privileged_extensions_custom_scripts_path);

extern void
handle_alter_extension(void (*process_utility_hook)(PROCESS_UTILITY_PARAMS),
                       PROCESS_UTILITY_PARAMS, char *privileged_extensions,
                       char *privileged_extensions_superuser);

extern void
handle_drop_extension(void (*process_utility_hook)(PROCESS_UTILITY_PARAMS),
                      PROCESS_UTILITY_PARAMS, char *privileged_extensions,
                      char *privileged_extensions_superuser);

#endif
