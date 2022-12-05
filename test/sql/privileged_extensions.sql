-- non-superuser extensions role
create role extensions_role login;
alter default privileges for role postgres in schema public grant all on tables to extensions_role;
set role extensions_role;
\echo

-- can create a privileged extension
create extension hstore;
select '1=>2'::hstore;

drop extension hstore;
\echo

-- custom scripts are run
select * from t2;

reset role;
drop table t2;
set role extensions_role;
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
create extension moddatetime;
