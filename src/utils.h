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

/**
 * Oid of the role used for elevation: `supautils.superuser` when set,
 * otherwise the bootstrap superuser.
 */
extern Oid superuser_oid(const char *superuser);

/**
 * Run the statements in `...` with the current user and security context set
 * to `uid` and `sec_context`, then restore the caller's, whether the body
 * completes or errors. The saved state lives on the stack. Don't nest one
 * call inside another's body (PG_TRY can't be nested in one scope before
 * PG14), and don't `return` or `break` out of the body.
 */
// clang-format off
#define RUN_AS(uid, sec_context, ...)                                          \
  do {                                                                         \
    Oid _prev_uid;                                                             \
    int _prev_sec_context;                                                     \
    GetUserIdAndSecContext(&_prev_uid, &_prev_sec_context);                    \
    SetUserIdAndSecContext((uid), (sec_context));                              \
    PG_TRY();                                                                  \
    {                                                                          \
      __VA_ARGS__                                                              \
    }                                                                          \
    PG_CATCH();                                                                \
    {                                                                          \
      SetUserIdAndSecContext(_prev_uid, _prev_sec_context);                    \
      PG_RE_THROW();                                                           \
    }                                                                          \
    PG_END_TRY();                                                              \
    SetUserIdAndSecContext(_prev_uid, _prev_sec_context);                      \
  } while (0)
// clang-format on

/**
 * Run the statements in `...` as the elevation role, in a restricted security
 * context, then restore the caller's role.
 */
#define RUN_ELEVATED(superuser, ...)                                           \
  do {                                                                         \
    Oid _uid;                                                                  \
    int _sec_context;                                                          \
    GetUserIdAndSecContext(&_uid, &_sec_context);                              \
    RUN_AS(superuser_oid(superuser),                                           \
           _sec_context | SECURITY_LOCAL_USERID_CHANGE |                       \
               SECURITY_RESTRICTED_OPERATION,                                  \
           __VA_ARGS__);                                                       \
  } while (0)

/**
 * Returns `false` if either s1 or s2 is NULL.
 */
extern bool is_string_in_comma_delimited_string(const char *s1, const char *s2);

extern bool remove_ending_wildcard(char *);

extern bool is_table_in_grant_list(char *const *table_names,
                                   size_t total_tables, Oid target_table_id);

typedef enum { ALT_FDW, ALT_PUB, ALT_EVTRIG } altered_obj_type;

extern void alter_owner(const char *obj_name, Oid role_oid,
                        altered_obj_type obj_type);

extern void destroyStringInfo(StringInfo str);

#endif
