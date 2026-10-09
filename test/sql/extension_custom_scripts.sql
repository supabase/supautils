set role extensions_role;
\echo

-- per-extension custom scripts are run
drop extension if exists citext;
create extension autoinc;

drop extension citext;
drop extension autoinc;
\echo

-- per-extension custom scripts are run for extensions not in privileged_extensions
create extension fuzzystrmatch;
drop extension fuzzystrmatch;
select * from t2;

reset role;
drop table t2;
set role extensions_role;
\echo

-- global extension custom scripts are run
create schema extensions;
show search_path;
create extension dict_xsyn;
select extnamespace::regnamespace from pg_extension where extname = 'dict_xsyn';
reset role;
create extension insert_username version "1.0" schema public cascade;
select extnamespace::regnamespace from pg_extension where extname = 'insert_username';
set role extensions_role;
show search_path;
drop schema extensions CASCADE;
\echo

-- custom scripts are run even for superusers
reset role;
create extension fuzzystrmatch;
drop extension fuzzystrmatch;
select * from t2;

drop table t2;
set role extensions_role;
\echo

-- test that pg_visibiliy is placed in visibility schema (test of example in docs)
reset role;
show search_path;
create extension pg_visibility;
select extnamespace::regnamespace from pg_extension where extname = 'pg_visibility';
show search_path;
drop extension pg_visibility;
set role extensions_role;

-- validate script fire order
reset role;
create extension btree_gin;
drop extension btree_gin;
set role extensions_role;
