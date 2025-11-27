#ifndef UTILS_H
#define UTILS_H

#include <postgres.h>

#include <catalog/pg_authid.h>
#include <commands/user.h>
#include <miscadmin.h>
#include <nodes/params.h>
#include <tcop/dest.h>
#include <tcop/utility.h>
#include <utils/acl.h>
#include <utils/queryenvironment.h>

#define PG14_GTE (PG_VERSION_NUM >= 140000)
#define PG15_GTE (PG_VERSION_NUM >= 150000)
#define PG16_GTE (PG_VERSION_NUM >= 160000)
#define PG17_GTE (PG_VERSION_NUM >= 170000)

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

#  define SUPAUTILS_EVENT_TRIGGER_OID EVENT_TRIGGEROID

#else // PG13

#  define PROCESS_UTILITY_PARAMS                                               \
    PlannedStmt *pstmt, const char *queryString,                               \
        ProcessUtilityContext context, ParamListInfo params,                   \
        QueryEnvironment *queryEnv, DestReceiver *dest, QueryCompletion *qc
#  define PROCESS_UTILITY_ARGS                                                 \
    pstmt, queryString, context, params, queryEnv, dest, qc

#  define SUPAUTILS_EVENT_TRIGGER_OID EVTTRIGGEROID

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
  { run_process_utility_hook(process_utility_hook); }                                     \
  PG_CATCH();                                                                  \
  {                                                                            \
    if (!(already_switched_to_superuser)) {                                      \
      switch_to_original_role();                                               \
    }                                                                          \
    PG_RE_THROW();                                                             \
  }                                                                            \
  PG_END_TRY();

// helper for testing a guc config
#if TEST
#  define SUPAUTILS_GUC_CONTEXT_SIGHUP PGC_USERSET
#else
#  define SUPAUTILS_GUC_CONTEXT_SIGHUP PGC_SIGHUP
#endif

/**
 * Switch to a superuser and save the original role. Caller is responsible for
 * calling switch_to_original_role() afterwards.
 */
extern void switch_to_superuser(const char *superuser, bool *already_switched);

/**
 * Restore the saved original role. Caller is responsible for ensuring
 * switch_to_superuser() was called.
 */
extern void switch_to_original_role(void);

/**
 * Returns `false` if either s1 or s2 is NULL.
 */
extern bool is_string_in_comma_delimited_string(const char *s1, const char *s2);

extern bool remove_ending_wildcard(char *);

typedef enum { ALT_FDW, ALT_PUB, ALT_EVTRIG } altered_obj_type;

extern void alter_owner(const char *obj_name, Oid role_oid,
                        altered_obj_type obj_type);

#endif
