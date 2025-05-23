set role extensions_role;
\echo

-- can create a privileged extension
create extension hstore;
select '1=>2'::hstore;

drop extension hstore;
\echo

-- cannot create other extensions
create extension file_fdw;
\echo

-- original role is restored on nested switch_to_superuser()
create extension autoinc;
select current_role;
\echo

-- can force sslinfo to be installed in pg_catalog
create extension sslinfo schema public;
select extnamespace::regnamespace from pg_extension where extname = 'sslinfo';

drop extension sslinfo;
\echo

-- switch to supautils.superuser even if superuser
reset role;
create role another_superuser superuser;
set role another_superuser;
create extension sslinfo;
select extowner::regrole from pg_extension where extname = 'sslinfo';

reset role;
drop extension sslinfo;
drop role another_superuser;
set role extensions_role;
\echo

-- can change extensions schema
create extension pageinspect;

select count(*) = 3 as extensions_in_public_schema
from information_schema.routines
where routine_name in ('page_header', 'heap_page_items', 'bt_metap')
and routine_schema = 'public';

-- go back to postgres role for creating a new schema and switch to extensions_role again
reset role;
create schema xtens;
set role extensions_role;
\echo

-- now alter extension schema
alter extension pageinspect set schema xtens;

select count(*) = 3 as extensions_in_xtens_schema
from information_schema.routines
where routine_name in ('page_header', 'heap_page_items', 'bt_metap')
and routine_schema = 'xtens';

-- users can change tables schemas normally
reset role;
set role nonsuper;

create table public.qux();
create schema baz;
alter table public.qux set schema baz;
select * from baz.qux;
