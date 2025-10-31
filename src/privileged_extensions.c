#include "privileged_extensions.h"

bool all_extensions_are_privileged(List       *objects,
                                   const char *privileged_extensions) {
  ListCell *lc;

  if (privileged_extensions == NULL) return false;

  foreach (lc, objects) {
    char *name = strVal(lfirst(lc));

    if (!is_string_in_comma_delimited_string(name, privileged_extensions)) {
      return false;
    }
  }

  return true;
}

bool is_extension_privileged(const char *extname,
                             const char *privileged_extensions) {
  if (privileged_extensions == NULL) return false;

  return is_string_in_comma_delimited_string(extname, privileged_extensions);
}
