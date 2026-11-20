#include "supautils_pgstat.h"


static const PgStat_KindInfo privilege_escalation_stats = {
	.name = "supautils_privilege_escalation",
	.fixed_amount = true,		/* exactly one entry */
	.write_to_file = true,		/* persist to stats file */

	.shared_size = sizeof(SupautilsPgStatShared_PrivilegeEscalationEntry),
	.shared_data_off = offsetof(SupautilsPgStatShared_PrivilegeEscalationEntry, stats),
	.shared_data_len = sizeof(((SupautilsPgStatShared_PrivilegeEscalationEntry *) 0)->stats),

	.init_shmem_cb = supautils_stats_privilege_escalation_init_shmem_cb,
	.reset_all_cb = supautils_stats_privilege_escalation_reset_all_cb,
	.snapshot_cb = supautils_stats_privilege_escalation_snapshot_cb,
};

void supautils_register_stats (void) {
  pgstat_register_kind(PGSTAT_KIND_RUN_ELEVATED_STATS, &privilege_escalation_stats);
}

/*
 * test_custom_stats_fixed_init_shmem_cb
 *		Initialize shared memory structure
 */
void
supautils_stats_privilege_escalation_init_shmem_cb(void *stats)
{
	SupautilsPgStatShared_PrivilegeEscalationEntry *stats_shmem =
		(SupautilsPgStatShared_PrivilegeEscalationEntry *) stats;

	LWLockInitialize(&stats_shmem->lock, LWTRANCHE_PGSTATS_DATA);
}

/*
 * test_custom_stats_fixed_reset_all_cb
 *		Reset the fixed-sized stats
 */
void
supautils_stats_privilege_escalation_reset_all_cb(TimestampTz ts)
{
	SupautilsPgStatShared_PrivilegeEscalationEntry *stats_shmem =
		pgstat_get_custom_shmem_data(PGSTAT_KIND_RUN_ELEVATED_STATS);

	/* see explanation above PgStatShared_Archiver for the reset protocol */
	LWLockAcquire(&stats_shmem->lock, LW_EXCLUSIVE);
	pgstat_copy_changecounted_stats(&stats_shmem->reset_offset,
									&stats_shmem->stats,
									sizeof(stats_shmem->stats),
									&stats_shmem->changecount);
	stats_shmem->stats.stat_reset_timestamp = ts;
	LWLockRelease(&stats_shmem->lock);
}

/*
 * test_custom_stats_fixed_snapshot_cb
 *		Copy current stats to snapshot area
 */
void
supautils_stats_privilege_escalation_snapshot_cb(void)
{
	SupautilsPgStatShared_PrivilegeEscalationEntry *stats_shmem =
		pgstat_get_custom_shmem_data(PGSTAT_KIND_RUN_ELEVATED_STATS);
	SupautilsPgStat_StatPrivilegeEscalationEntry *stat_snap =
		pgstat_get_custom_snapshot_data(PGSTAT_KIND_RUN_ELEVATED_STATS);
	SupautilsPgStat_StatPrivilegeEscalationEntry *reset_offset = &stats_shmem->reset_offset;
	SupautilsPgStat_StatPrivilegeEscalationEntry reset;

	pgstat_copy_changecounted_stats(stat_snap,
									&stats_shmem->stats,
									sizeof(stats_shmem->stats),
									&stats_shmem->changecount);

	LWLockAcquire(&stats_shmem->lock, LW_SHARED);
	memcpy(&reset, reset_offset, sizeof(stats_shmem->stats));
	LWLockRelease(&stats_shmem->lock);

	/* Apply reset offsets */
#define FIXED_COMP(fld) stat_snap->fld -= reset.fld;
	FIXED_COMP(numcalls);
#undef FIXED_COMP
}

void stats_run_elevated_update(void){
  SupautilsPgStatShared_PrivilegeEscalationEntry *stats_shmem;
  stats_shmem = pgstat_get_custom_shmem_data(PGSTAT_KIND_RUN_ELEVATED_STATS);
  LWLockAcquire(&stats_shmem->lock, LW_EXCLUSIVE);
  pgstat_begin_changecount_write(&stats_shmem->changecount);
  stats_shmem->stats.numcalls++;
  pgstat_end_changecount_write(&stats_shmem->changecount);
  LWLockRelease(&stats_shmem->lock);
}

PG_FUNCTION_INFO_V1(stats_run_elevated);
Datum
stats_run_elevated(__attribute__((unused)) PG_FUNCTION_ARGS)
{
	TupleDesc	tupdesc;
	Datum		values[2] = {0};
	bool		nulls[2] = {false};
	SupautilsPgStat_StatPrivilegeEscalationEntry *stats;

	/* Take snapshot (applies reset offsets) */
	pgstat_snapshot_fixed(PGSTAT_KIND_RUN_ELEVATED_STATS);
	stats = pgstat_get_custom_snapshot_data(PGSTAT_KIND_RUN_ELEVATED_STATS);

	/* Build return tuple */
	tupdesc = CreateTemplateTupleDesc(2);
	TupleDescInitEntry(tupdesc, (AttrNumber) 1, "numcalls",
					   INT8OID, -1, 0);
	TupleDescInitEntry(tupdesc, (AttrNumber) 2, "stats_reset",
					   TIMESTAMPTZOID, -1, 0);
	TupleDescFinalize(tupdesc);
	BlessTupleDesc(tupdesc);

	values[0] = Int64GetDatum(stats->numcalls);

	/* Handle uninitialized timestamp (no reset yet) */
	if (stats->stat_reset_timestamp == 0)
	{
		nulls[1] = true;
	}
	else
	{
		values[1] = TimestampTzGetDatum(stats->stat_reset_timestamp);
	}

	/* Return as tuple */
	PG_RETURN_DATUM(HeapTupleGetDatum(heap_form_tuple(tupdesc, values, nulls)));
}
