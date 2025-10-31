#ifndef PRIVILEGED_EXTENSIONS_H
#define PRIVILEGED_EXTENSIONS_H

#include "pg_prelude.h"
#include "utils.h"

extern bool all_extensions_are_privileged(List       *objects,
                                          const char *privileged_extensions);

extern bool is_extension_privileged(const char *extname,
                                    const char *privileged_extensions);

#endif
