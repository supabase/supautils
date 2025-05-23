#ifndef EXTENSION_CUSTOM_SCRIPTS_H
#define EXTENSION_CUSTOM_SCRIPTS_H

#include "pg_prelude.h"

extern void run_global_before_create_script(
    char *extname, List *options,
    const char *privileged_extensions_custom_scripts_path);

extern void run_ext_before_create_script(
    char *extname, List *options,
    const char *privileged_extensions_custom_scripts_path);

extern void run_ext_after_create_script(
    char *extname, List *options,
    const char *privileged_extensions_custom_scripts_path);

#endif
