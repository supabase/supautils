#include "pg_prelude.h"
#include "event_triggers.h"

PG_FUNCTION_INFO_V1(noop);
Datum noop(__attribute__ ((unused)) PG_FUNCTION_ARGS) { PG_RETURN_VOID();}

void
force_noop(FmgrInfo *finfo)
{
    finfo->fn_addr   = (PGFunction) noop;
    finfo->fn_oid    = 38;                   /* put the int2in oid which is sure to exist, this avoids cache lookup errors. See https://github.com/supabase/supautils/pull/129*/
    finfo->fn_nargs  = 0;                    /* no arguments for noop */
    finfo->fn_strict = false;
    finfo->fn_retset = false;
    finfo->fn_stats  = 0;                    /* no stats collection */
    finfo->fn_extra  = NULL;                 /* clear out old context data */
    finfo->fn_mcxt   = CurrentMemoryContext;
    finfo->fn_expr   = NULL;                 /* no parse tree */
}

Oid get_function_owner(func_owner_search search){
  // Lookup function name OID. Note that for event trigger functions, there's no arguments.
  Oid func_oid = InvalidOid;

  switch(search.as){
  case FO_SEARCH_NAME:
    func_oid = LookupFuncName(search.val.funcname, 0, NULL, false);
    break;
  case FO_SEARCH_FINFO:
    func_oid = search.val.finfo->fn_oid;
    break;
  }

  HeapTuple proc_tup = SearchSysCache1(PROCOID, ObjectIdGetDatum(func_oid));
  if (!HeapTupleIsValid(proc_tup))
      ereport(ERROR,
              (errmsg("cache lookup failed for function %u", func_oid)));

  Form_pg_proc procForm = (Form_pg_proc) GETSTRUCT(proc_tup);
  Oid func_owner = procForm->proowner;

  ReleaseSysCache(proc_tup);

  return func_owner;
}

