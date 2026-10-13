-- unset by default, create extension behaves as stock postgres
show supautils.default_extension_install_schema;
create extension hstore;
select extnamespace::regnamespace from pg_extension where extname = 'hstore';
drop extension hstore;
\echo

-- can be set per session by a superuser
set supautils.default_extension_install_schema to 'extensions';
\echo

-- falls back to stock behavior while the target schema does not exist
create extension hstore;
select extnamespace::regnamespace from pg_extension where extname = 'hstore';
drop extension hstore;
\echo

create schema extensions;
\echo

-- schema-less create extension lands in the default schema, search_path is untouched
show search_path;
create extension hstore;
select extnamespace::regnamespace from pg_extension where extname = 'hstore';
show search_path;
drop extension hstore;
\echo

-- an explicit schema clause wins over the default
create extension hstore schema public;
select extnamespace::regnamespace from pg_extension where extname = 'hstore';
drop extension hstore;
\echo

-- a control-file pinned schema wins over the default (pg_tle pins pgtle)
create extension pg_tle;
select extnamespace::regnamespace from pg_extension where extname = 'pg_tle';
drop extension pg_tle;
drop schema pgtle;
\echo

-- an extensions_parameter_overrides entry wins over the default
-- (sslinfo is pinned to pg_catalog in init.conf.in)
create extension sslinfo;
select extnamespace::regnamespace from pg_extension where extname = 'sslinfo';
drop extension sslinfo;
\echo

-- works with an empty search_path
set search_path = '';
create extension hstore;
select extnamespace::regnamespace from pg_extension where extname = 'hstore';
show search_path;
drop extension hstore;
reset search_path;
\echo

-- search_path is untouched when create extension errors
create extension does_not_exist;
show search_path;
\echo

-- does not leak to later statements of the same transaction
begin;
create extension hstore;
create table t_default_schema_leak();
commit;
select schemaname from pg_tables where tablename = 't_default_schema_leak';
drop table t_default_schema_leak;
drop extension hstore;
\echo

-- a set local before create extension still reverts at commit
begin;
set local search_path = pg_catalog;
create extension hstore;
select extnamespace::regnamespace from pg_extension where extname = 'hstore';
show search_path;
commit;
show search_path;
drop extension hstore;
\echo

-- schema names that need quoting work
create schema "Mixed Case";
set supautils.default_extension_install_schema to 'Mixed Case';
create extension hstore;
select extnamespace::regnamespace from pg_extension where extname = 'hstore';
drop extension hstore;
drop schema "Mixed Case";
set supautils.default_extension_install_schema to 'extensions';
\echo

-- applies to privileged extensions created by non-superusers
set role extensions_role;
create extension hstore;
select extnamespace::regnamespace from pg_extension where extname = 'hstore';
drop extension hstore;
\echo

-- non-superusers cannot change the setting
set supautils.default_extension_install_schema to 'public';
reset role;
\echo

-- cleanup
drop schema extensions;
