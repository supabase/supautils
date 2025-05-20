#ifndef PRIVILEGED_EXTENSIONS_H
#define PRIVILEGED_EXTENSIONS_H

#include "utils.h"

bool all_extensions_are_privileged(List *objects, const char *privileged_extensions);

bool is_extension_privileged(const char *extname, const char *privileged_extensions);

void run_global_before_create_script(char *extname, List *options, const char *privileged_extensions_custom_scripts_path);

void run_ext_before_create_script(char *extname, List *options, const char *privileged_extensions_custom_scripts_path);

void run_ext_after_create_script(char *extname, List *options, const char *privileged_extensions_custom_scripts_path);

#endif
