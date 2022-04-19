#ifndef UTILS_H
#define UTILS_H

#include <postgres.h>

#include <nodes/params.h>
#include <tcop/dest.h>
#include <tcop/utility.h>
#include <utils/queryenvironment.h>

#define PG13_GTE (PG_VERSION_NUM >= 130000)
#define PG14_GTE (PG_VERSION_NUM >= 140000)

#if PG14_GTE

#define PROCESS_UTILITY_PARAMS                                                 \
    PlannedStmt *pstmt, const char *queryString, bool readOnlyTree,            \
        ProcessUtilityContext context, ParamListInfo params,                   \
        QueryEnvironment *queryEnv, DestReceiver *dest, QueryCompletion *qc
#define PROCESS_UTILITY_ARGS                                                   \
    pstmt, queryString, readOnlyTree, context, params, queryEnv, dest, qc

#elif PG13_GTE

#define PROCESS_UTILITY_PARAMS                                                 \
    PlannedStmt *pstmt, const char *queryString,                               \
        ProcessUtilityContext context, ParamListInfo params,                   \
        QueryEnvironment *queryEnv, DestReceiver *dest, QueryCompletion *qc
#define PROCESS_UTILITY_ARGS                                                   \
    pstmt, queryString, context, params, queryEnv, dest, qc

#else

#define PROCESS_UTILITY_PARAMS                                                 \
    PlannedStmt *pstmt, const char *queryString,                               \
        ProcessUtilityContext context, ParamListInfo params,                   \
        QueryEnvironment *queryEnv, DestReceiver *dest, char *completionTag

#define PROCESS_UTILITY_ARGS                                                   \
    pstmt, queryString, context, params, queryEnv, dest, completionTag

#endif

#define run_process_utility_hook(process_utility_hook)                         \
    if (process_utility_hook != NULL) {                                        \
        process_utility_hook(PROCESS_UTILITY_ARGS);                            \
    } else {                                                                   \
        standard_ProcessUtility(PROCESS_UTILITY_ARGS);                         \
    }

#endif
