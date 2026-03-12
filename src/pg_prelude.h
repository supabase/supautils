#ifndef PG_PRELUDE_H
#define PG_PRELUDE_H

// pragmas needed to pass compiling with -Wextra
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wsign-compare"

#include <postgres.h>

#include <access/htup_details.h>
#include <access/xact.h>
#include <catalog/namespace.h>
#include <catalog/pg_authid.h>
#include <catalog/pg_collation_d.h>
#include <catalog/pg_proc.h>
#include <commands/defrem.h>
#include <commands/event_trigger.h>
#include <commands/publicationcmds.h>
#include <common/jsonapi.h>
#include <executor/executor.h>
#include <executor/spi.h>
#include <fmgr.h>
#include <miscadmin.h>
#include <nodes/bitmapset.h>
#include <nodes/makefuncs.h>
#include <nodes/parsenodes.h>
#include <nodes/pg_list.h>
#include <nodes/value.h>
#include <parser/parse_func.h>
#include <tcop/utility.h>
#include <tsearch/ts_locale.h>
#include <utils/acl.h>
#include <utils/builtins.h>
#include <utils/fmgrprotos.h>
#include <utils/formatting.h>
#include <utils/guc.h>
#include <utils/guc_tables.h>
#include <utils/json.h>
#include <utils/jsonb.h>
#include <utils/jsonfuncs.h>
#include <utils/lsyscache.h>
#include <utils/memutils.h>
#include <utils/regproc.h>
#include <utils/snapmgr.h>
#include <utils/syscache.h>
#include <utils/varlena.h>

#pragma GCC diagnostic pop

#define PG14_GTE (PG_VERSION_NUM >= 140000)
#define PG15_GTE (PG_VERSION_NUM >= 150000)
#define PG16_GTE (PG_VERSION_NUM >= 160000)
#define PG17_GTE (PG_VERSION_NUM >= 170000)
#define PG17_LT (PG_VERSION_NUM < 170000)
#define PG16_LT (PG_VERSION_NUM < 160000)

#if PG17_GTE

#  define NEW_JSON_LEX_CONTEXT_CSTRING_LEN(a, b, c, d)                         \
    makeJsonLexContextCstringLen(NULL, a, b, c, d)

#else

#  define NEW_JSON_LEX_CONTEXT_CSTRING_LEN(a, b, c, d)                         \
    makeJsonLexContextCstringLen(a, b, c, d)

#endif

#if PG16_GTE

#  define JSON_ACTION_RETURN_TYPE JsonParseErrorType
#  define JSON_ACTION_RETURN return JSON_SUCCESS

#else

#  define JSON_ACTION_RETURN_TYPE void
#  define JSON_ACTION_RETURN return

#endif

#if PG14_GTE

#  define PROCESS_UTILITY_PARAMS                                               \
    PlannedStmt *pstmt, const char *queryString, bool readOnlyTree,            \
        ProcessUtilityContext context, ParamListInfo params,                   \
        QueryEnvironment *queryEnv, DestReceiver *dest, QueryCompletion *qc
#  define PROCESS_UTILITY_ARGS                                                 \
    pstmt, queryString, readOnlyTree, context, params, queryEnv, dest, qc

#else // PG13 and below

#  define PROCESS_UTILITY_PARAMS                                               \
    PlannedStmt *pstmt, const char *queryString,                               \
        ProcessUtilityContext context, ParamListInfo params,                   \
        QueryEnvironment *queryEnv, DestReceiver *dest, QueryCompletion *qc
#  define PROCESS_UTILITY_ARGS                                                 \
    pstmt, queryString, context, params, queryEnv, dest, qc

// The EVENT_TRIGGEROID was called EVTTRIGGEROID prior pg 14
#  define EVENT_TRIGGEROID EVTTRIGGEROID

#endif

#define run_process_utility_hook(process_utility_hook)                         \
  if (process_utility_hook != NULL) {                                          \
    process_utility_hook(PROCESS_UTILITY_ARGS);                                \
  } else {                                                                     \
    standard_ProcessUtility(PROCESS_UTILITY_ARGS);                             \
  }

#define run_process_utility_hook_with_cleanup(process_utility_hook,            \
                                              already_switched_to_superuser,   \
                                              switch_to_original_role)         \
  PG_TRY();                                                                    \
  {                                                                            \
    run_process_utility_hook(process_utility_hook);                            \
  }                                                                            \
  PG_CATCH();                                                                  \
  {                                                                            \
    if (!(already_switched_to_superuser)) {                                    \
      switch_to_original_role();                                               \
    }                                                                          \
    PG_RE_THROW();                                                             \
  }                                                                            \
  PG_END_TRY();

// polyfill
#if PG17_LT

#  define foreach_internal(type, pointer, var, lst, func)                      \
    for (type pointer var = 0, pointer var##__outerloop = (type pointer)1;     \
         var##__outerloop; var##__outerloop = 0)                               \
      for (ForEachState var##__state = {(lst), 0};                             \
           (var##__state.l != NIL &&                                           \
            var##__state.i < var##__state.l->length &&                         \
            (var = (type pointer)func(                                         \
                 &var##__state.l->elements[var##__state.i]),                   \
            true));                                                            \
           var##__state.i++)

#  define foreach_ptr(type, var, lst)                                          \
    foreach_internal(type, *, var, lst, lfirst)

#endif

#endif /* PG_PRELUDE_H */
