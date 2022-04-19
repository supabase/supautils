#ifndef ALLOWED_EXTENSIONS_H
#define ALLOWED_EXTENSIONS_H

#include "utils.h"

extern void
handle_create_extension(void (*process_utility_hook)(PROCESS_UTILITY_PARAMS),
                        PROCESS_UTILITY_PARAMS, CreateExtensionStmt *stmt,
                        char *allowed_extensions, char *extensions_superuser);
extern void
handle_alter_extension(void (*process_utility_hook)(PROCESS_UTILITY_PARAMS),
                       PROCESS_UTILITY_PARAMS, AlterExtensionStmt *stmt,
                       char *allowed_extensions, char *extensions_superuser);
extern void
handle_drop_extension(void (*process_utility_hook)(PROCESS_UTILITY_PARAMS),
                      PROCESS_UTILITY_PARAMS, DropStmt *stmt,
                      char *allowed_extensions, char *extensions_superuser);

#endif
