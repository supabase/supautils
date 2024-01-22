-- non-superuser extensions role
create role extensions_role login;
grant all on database postgres to extensions_role;
alter default privileges for role postgres in schema public grant all on tables to extensions_role;
set role extensions_role;
\echo

-- can create a privileged extension
create extension hstore;
select '1=>2'::hstore;

drop extension hstore;
\echo

-- per-extension custom scripts are run
select * from t2;

reset role;
drop table t2;
set role extensions_role;
\echo

-- global extension custom scripts are run
create extension pg_tle;
reset role;
-- must run this after `create extension pg_tle` since the role only exists
-- after the ext is created
grant pgtle_admin to extensions_role;
set role extensions_role;
select pgtle.install_extension('foo', '1', '', 'select 1', '{}');
create extension foo cascade;

drop extension pg_tle cascade;
\echo

-- custom scripts are run even for superusers
reset role;
create extension hstore;
drop extension hstore;
select * from t2;

drop table t2;
set role extensions_role;
\echo

-- cannot create other extensions
create extension file_fdw;
\echo

-- original role is restored on nested switch_to_superuser()
create extension autoinc;
select current_role;
