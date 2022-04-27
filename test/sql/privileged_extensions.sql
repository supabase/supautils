-- non-superuser extensions role
create role extensions_role login;
alter default privileges for role postgres in schema public grant all on tables to extensions_role;
set role extensions_role;
\echo

-- can create a privileged extension
create extension hstore;
select '1=>2'::hstore;
\echo

-- can run custom scripts
select * from t2;
\echo

-- cannot create other extensions
create extension moddatetime;
