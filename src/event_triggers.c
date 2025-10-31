#include "pg_prelude.h"

#include "event_triggers.h"
#include "utils.h"

// this is the underlying function of `select version();`
extern Datum pgsql_version(PG_FUNCTION_ARGS);

void force_noop(FmgrInfo *finfo) {
  finfo->fn_addr = (PGFunction)pgsql_version;
  finfo->fn_oid  = 89; /* this is the oid of pgsql_version function, it's stable
                          and keeps being the  same on latest pg version */
  finfo->fn_nargs  = 0; /* no arguments for version() */
  finfo->fn_strict = false;
  finfo->fn_retset = false;
  finfo->fn_stats  = 0;    /* no stats collection */
  finfo->fn_extra  = NULL; /* clear out old context data */
  finfo->fn_mcxt   = CurrentMemoryContext;
  finfo->fn_expr   = NULL; /* no parse tree */
}

func_attrs get_function_attrs(func_search search) {
  // Lookup function name OID. Note that for event trigger functions, there's no
  // arguments.
  Oid func_oid = InvalidOid;

  switch (search.as) {
  case FO_SEARCH_NAME:
    func_oid = LookupFuncName(search.val.funcname, 0, NULL, false);
    break;
  case FO_SEARCH_FINFO: func_oid = search.val.finfo->fn_oid; break;
  }

  HeapTuple proc_tup = SearchSysCache1(PROCOID, ObjectIdGetDatum(func_oid));
  if (!HeapTupleIsValid(proc_tup))
    ereport(ERROR, (errmsg("cache lookup failed for function %u", func_oid)));

  Form_pg_proc procForm   = (Form_pg_proc)GETSTRUCT(proc_tup);
  Oid          func_owner = procForm->proowner;
  bool         is_secdef  = procForm->prosecdef;

  ReleaseSysCache(proc_tup);

  return (func_attrs){func_owner, is_secdef};
}

bool is_event_trigger_function(Oid foid) {
  return get_func_rettype(foid) == SUPAUTILS_EVENT_TRIGGER_OID;
}
