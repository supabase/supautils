#include "protected_extensions.h"

bool is_extension_protected(const char *extname,
                            const char *protected_extensions) {
  if (protected_extensions == NULL) return false;

  return is_string_in_comma_delimited_string(extname, protected_extensions);
}

const char *first_protected_extension(List       *objects,
                                      const char *protected_extensions) {
  ListCell *lc;

  if (protected_extensions == NULL) return NULL;

  foreach (lc, objects) {
    const char *name = strVal(lfirst(lc));

    // an extension that isn't installed can't be protected, so that
    // DROP EXTENSION IF EXISTS keeps its "does not exist, skipping" notice
    if (is_extension_protected(name, protected_extensions) &&
        OidIsValid(get_extension_oid(name, true)))
      return name;
  }

  return NULL;
}

/*
 * Walks pg_depend from the extensions in `objects` to every extension that
 * requires them, at any depth. A protected extension in that set would be
 * dropped by DROP EXTENSION ... CASCADE on the target.
 */
const char *protected_dependent_extension(List        *objects,
                                          const char  *protected_extensions,
                                          const char **required) {
  ListCell      *lc;
  const char    *found = NULL;
  StringInfoData sql;
  int            ret;

  *required = NULL;

  if (protected_extensions == NULL || objects == NIL) return NULL;

  Assert(ActiveSnapshotSet());

  PushActiveSnapshot(GetTransactionSnapshot());

  if ((ret = SPI_connect()) != SPI_OK_CONNECT)
    elog(ERROR,
         "SPI_connect failed when getting dependent extensions with error code "
         "%d",
         ret);

  initStringInfo(&sql);
  appendStringInfoString(
      &sql,
      "with recursive deps(oid, root) as ("
      "  select oid, extname from pg_catalog.pg_extension where extname in (");

  bool first = true;
  foreach (lc, objects) {
    if (!first) appendStringInfoString(&sql, ", ");
    appendStringInfoString(&sql, quote_literal_cstr(strVal(lfirst(lc))));
    first = false;
  }

  appendStringInfoString(
      &sql, ")"
            "  union"
            "  select d.objid, deps.root from pg_catalog.pg_depend d"
            "  join deps on d.refobjid = deps.oid"
            "  where d.classid = 'pg_catalog.pg_extension'::regclass"
            "  and d.refclassid = 'pg_catalog.pg_extension'::regclass"
            ")"
            " select e.extname, deps.root from pg_catalog.pg_extension e"
            " join deps on e.oid = deps.oid where e.extname <> deps.root");

  ret = SPI_execute(sql.data, true, 0);

  if (ret != SPI_OK_SELECT)
    elog(ERROR,
         "SPI_execute failed when getting dependent extensions with error code "
         "%d",
         ret);

  for (uint64 i = 0; i < SPI_processed; i++) {
    char *extname =
        SPI_getvalue(SPI_tuptable->vals[i], SPI_tuptable->tupdesc, 1);

    if (is_extension_protected(extname, protected_extensions)) {
      found     = MemoryContextStrdup(CurTransactionContext, extname);
      *required = MemoryContextStrdup(
          CurTransactionContext,
          SPI_getvalue(SPI_tuptable->vals[i], SPI_tuptable->tupdesc, 2));
      break;
    }
  }

  pfree(sql.data);

  if ((ret = SPI_finish()) != SPI_OK_FINISH)
    elog(ERROR,
         "SPI_finish failed when getting dependent extensions with error code "
         "%d",
         ret);

  PopActiveSnapshot();

  return found;
}
