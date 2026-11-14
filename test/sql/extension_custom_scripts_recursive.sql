-- This file tests the behaviour of running extension custom scripts
-- when they contain `create extension ...` commands which recursively
-- call the same extension custom scripts. We'd like to prevent
-- infinite (self or mutual) recursion when this happens. An example
-- of a self-recursive call is the following:
--
-- User runs a `create extension self_recursive` command when the
-- self_recursive extension's before-create.sql file also contains a
-- `create extension self_recursive` command.
--
-- If we are not careful, this would lead to infinite recursion as
-- follows:
--
-- 1. User's `create extension self_recursive` call is hooked via the
-- process utility hook which runs the before-create.sql script.
-- 2. The `create extension self_recursive` command from before-create.sql
-- call is hooked via the process utility hook which runs the 
-- before-create.sql script
-- 3. And so on ...
--
-- Same problem occurs if we have two or more extensions' custom scripts
-- having a `create extension ...` command referencing each other. For
-- example:
--
-- Extension `first`'s before-create.sql contains `create extension second`
-- and extension `second`'s before-create.sql contains
-- `create extension first`.
--
-- Equallly important, when there is no recursion, extensions should be 
-- created successfully. For example:
--
-- Extension `first`'s before-create.sql contains `create extension second`
-- but extension `second`'s before-create.sql doesn't contain any `create
-- extension ...` command.
--
-- Lastly, note that the cycle can start from any of the four scripts, both
-- global and per-extension before and after create scripts.
\echo

-- bloom extension is configured to be self-recursive in init.sh
create extension bloom;
\echo

-- pgrowlocks and pgstattuple are configured to be mutually recursive
-- in init.sh
create extension pgrowlocks;
\echo

create extension pgstattuple;
\echo

-- pg_buffercache, pg_visibility and pg_prewarm form a 3-cycle in init.sh
create extension if not exists pg_buffercache;
\echo

-- the cycle can be entered from any of its members
create extension if not exists pg_visibility;
\echo

-- and each one reports the correct cycle
create extension if not exists pg_prewarm;
\echo

-- Non cyclic dependency: amcheck's before-create.sql creates tsm_system_rows
-- This should run both extension's custom scripts without error and create 
-- them successfully
create extension if not exists amcheck;
select extname from pg_extension
where extname in ('amcheck', 'tsm_system_rows') order by extname;
drop extension amcheck, tsm_system_rows;
\echo

-- Test that only actual unconditonal `create extension ...` commands in the
-- global scripts create a cycle. See the global script in init.sh.
create extension tsm_system_time;
select extname from pg_extension
where extname in ('tsm_system_time', 'pg_surgery') order by extname;
drop extension tsm_system_time, pg_surgery;
