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
