
alter system set supautils.default_extension_install_schema to 'test_default_extension_install_schema_1';
alter system set supautils.extensions_parameter_overrides to '{}';
SELECT pg_reload_conf();
\connect
create schema test_default_extension_install_schema_1;
create schema test_default_extension_install_schema_2;
\echo

-- test explicit scheam option overrides default
create extension hstore schema test_default_extension_install_schema_2;
select extnamespace::regnamespace from pg_extension where extname = 'hstore';
drop extension hstore;

-- test that no option overrides default search path
set search_path TO test_default_extension_install_schema_2;
create extension hstore;
select extnamespace::regnamespace from pg_extension where extname = 'hstore';
drop extension hstore;
reset search_path;

-- test that extension schema override replaces default
alter system set supautils.extensions_parameter_overrides to '{"hstore": {"schema": "test_default_extension_install_schema_2"}}';
SELECT pg_reload_conf();
\connect
create extension hstore;
select extnamespace::regnamespace from pg_extension where extname = 'hstore';
drop extension hstore;
alter system reset supautils.extensions_parameter_overrides;

create extension pgmq;
select extnamespace::regnamespace from pg_extension where extname = 'pgmq';
drop extension pgmq;

drop schema test_default_extension_install_schema_1;
drop schema test_default_extension_install_schema_2;
alter system reset supautils.default_extension_install_schema;
alter system reset supautils.extensions_parameter_overrides;
SELECT pg_reload_conf();
\connect
