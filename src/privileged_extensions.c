#include "privileged_extensions.h"

bool all_extensions_are_privileged(List       *objects,
                                   const char *privileged_extensions) {
  ListCell *lc;

  if (privileged_extensions == NULL) return false;

  foreach (lc, objects) {
    char *name = strVal(lfirst(lc));

    if (!is_extension_privileged(name, privileged_extensions)) {
      return false;
    }
  }

  return true;
}

bool is_extension_privileged(const char *extname,
                             const char *privileged_extensions) {
  if (privileged_extensions == NULL) return false;

  return is_string_in_comma_delimited_string(extname, privileged_extensions) &&
         is_extension_available(extname);
}

/*
 * Returns true if the extension is present in the pg_available_extensions
 * view, false otherwise. Only those extensions are present in this view
 * which have their control files on disk.
 */
bool is_extension_available(const char *extname) {
  bool found = false;

  PushActiveSnapshot(GetTransactionSnapshot());
  SPI_connect();

  StringInfoData sql;

  initStringInfo(&sql);
  appendStringInfo(
      &sql, "select 1 from pg_catalog.pg_available_extensions where name = %s",
      quote_literal_cstr(extname));

  int rc = SPI_execute(sql.data, true, 1);

  if (rc != SPI_OK_SELECT) {
    elog(ERROR,
         "SPI_execute to get available extensions failed with error "
         "code %d",
         rc);
  }

  found = SPI_processed > 0;

  pfree(sql.data);
  SPI_finish();
  PopActiveSnapshot();

  return found;
}
