-- this feature is only enabled for pg >= 13, for pg12 this will be false
SELECT current_setting('server_version_num')::int >= 130000 as gte_pg13;

-- constrained by cpu
create extension adminpack;
\echo

-- constrained by memory
create extension cube;
\echo

-- constrained by disk
create extension lo;
\echo

-- passes all resource constraints
create extension amcheck;
\echo

-- no resource constraints
create extension bloom;
