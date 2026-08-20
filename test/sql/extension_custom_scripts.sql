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
create extension dict_xsyn;
reset role;
create extension insert_username version "1.0" schema public cascade;
set role extensions_role;
\echo

-- custom scripts are run even for superusers
reset role;
create extension fuzzystrmatch;
drop extension fuzzystrmatch;
select * from t2;

drop table t2;
set role extensions_role;
\echo

-- global state is restored when a custom script errors
-- (terse errors, the CONTEXT would contain the machine dependent scripts path)
\set VERBOSITY terse
create extension plls;
\set VERBOSITY default
\echo

-- the original role is restored after the error
select current_role;
\echo

-- elevation still works after the error
create extension hstore;
select '1=>2'::hstore;
drop extension hstore;

-- sslinfo can only be created by a superuser, so this proves supautils still
-- elevates after the failed custom script
create extension sslinfo;
select extowner::regrole from pg_extension where extname = 'sslinfo';
drop extension sslinfo;
\echo

-- custom scripts still run after the error
create extension fuzzystrmatch;
drop extension fuzzystrmatch;
select * from t2;

reset role;
drop table t2;
set role extensions_role;
\echo
