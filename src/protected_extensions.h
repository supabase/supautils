#ifndef PROTECTED_EXTENSIONS_H
#define PROTECTED_EXTENSIONS_H

#include "pg_prelude.h"
#include "utils.h"

#include <commands/extension.h>

extern bool is_extension_protected(const char *extname,
                                   const char *protected_extensions);

/**
 * Returns the name of the first extension in `objects` that is protected, or
 * NULL if none of them is.
 */
extern const char *first_protected_extension(List       *objects,
                                             const char *protected_extensions);

/**
 * Looks for a protected extension that depends, directly or through other
 * extensions, on any extension in `objects`. Returns its name and sets
 * `*required` to the extension in `objects` it depends on, or returns NULL
 * when there is none.
 */
extern const char *
protected_dependent_extension(List *objects, const char *protected_extensions,
                              const char **required);

#endif
