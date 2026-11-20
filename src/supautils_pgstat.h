#ifndef SUPAUTILS_PGSTAT_H
#define SUPAUTILS_PGSTAT_H

#include "pg_prelude.h"
#include "postgres.h"
#include "access/htup_details.h"
#include "funcapi.h"
#include "pgstat.h"
#include "utils/builtins.h"
#include "utils/pgstat_internal.h"
#include "utils/timestamp.h"

/*
 * Kind ID for test_custom_fixed_stats.
 */
#define PGSTAT_KIND_RUN_ELEVATED_STATS 26

/* Fixed-amount custom statistics entry */
typedef struct SupautilsPgStat_StatPrivilegeEscalationEntry
{
	PgStat_Counter numcalls;	/* # of times update function called */
	TimestampTz stat_reset_timestamp;
} SupautilsPgStat_StatPrivilegeEscalationEntry;

typedef struct SupautilsPgStatShared_PrivilegeEscalationEntry
{
	LWLock		lock;			/* protects counters */
	uint32		changecount;	/* for atomic reads */
	SupautilsPgStat_StatPrivilegeEscalationEntry stats;	/* current counters */
	SupautilsPgStat_StatPrivilegeEscalationEntry reset_offset;	/* reset baseline */
} SupautilsPgStatShared_PrivilegeEscalationEntry;

/* Callbacks for fixed-amount statistics */
void supautils_stats_privilege_escalation_init_shmem_cb(void *stats);
void supautils_stats_privilege_escalation_reset_all_cb(TimestampTz ts);
void supautils_stats_privilege_escalation_snapshot_cb(void);

extern void supautils_register_stats (void);

extern void stats_run_elevated_update(void);

#endif
