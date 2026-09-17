-- check json validation works
alter system set supautils.extensions_parameter_overrides to '';
alter system set supautils.extensions_parameter_overrides to '[]';
alter system set supautils.extensions_parameter_overrides to '1';
alter system set supautils.extensions_parameter_overrides to '{"sslinfo": 123}';
alter system set supautils.extensions_parameter_overrides to '{"sslinfo": {"schema": {}}}';
alter system set supautils.extensions_parameter_overrides to '{"sslinfo": {"version": "1.0"}}';
alter system set supautils.extensions_parameter_overrides to '{"sslinfo": {"schema": 123}}';
\echo

-- Upto 100 parameter overrides can be set successfully
select '{' || string_agg(format('"ext_%s":{"schema": "pg_catalog"}', i), ',') || '}' as overrides
from generate_series(1, 100) i \gset
\echo

alter system set supautils.extensions_parameter_overrides to :'overrides';
\echo

-- More than 100 raise an error instead
select '{' || string_agg(format('"ext_%s":{"schema": "pg_catalog"}', i), ',') || '}' as overrides
from generate_series(1, 101) i \gset
\echo

alter system set supautils.extensions_parameter_overrides to :'overrides';
\echo

-- can force sslinfo to be installed in pg_catalog
-- override for sslinfo extension to be installed in pg_catalog is set in init.conf.in file
create extension sslinfo schema public;
select extnamespace::regnamespace from pg_extension where extname = 'sslinfo';

drop extension sslinfo;
\echo

-- the schema override cannot be bypassed with ALTER EXTENSION ... SET SCHEMA
create schema other_schema;
grant all on schema other_schema to extensions_role, nonsuper;
\echo

-- as a non-superuser, for an extension in supautils.privileged_extensions
set role extensions_role;
create extension sslinfo schema public;
select extnamespace::regnamespace from pg_extension where extname = 'sslinfo';

alter extension sslinfo set schema other_schema;
select extnamespace::regnamespace from pg_extension where extname = 'sslinfo';

-- the extension is still usable
select ssl_is_used();
\echo

-- an extension without an override is not pinned
create extension hstore schema public;
alter extension hstore set schema other_schema;
select extnamespace::regnamespace from pg_extension where extname = 'hstore';

drop extension hstore;
reset role;
drop extension sslinfo;
\echo

-- as a superuser, since the override also applies to superusers on create
create extension sslinfo schema public;
select extnamespace::regnamespace from pg_extension where extname = 'sslinfo';

alter extension sslinfo set schema other_schema;
select extnamespace::regnamespace from pg_extension where extname = 'sslinfo';

drop extension sslinfo;
\echo

-- non-extension objects can still be moved between schemas
set role nonsuper;
create table public.override_tbl();
alter table public.override_tbl set schema other_schema;
select relnamespace::regnamespace from pg_class where relname = 'override_tbl';

drop table other_schema.override_tbl;
reset role;

drop schema other_schema;
