-- check json validation works
alter system set supautils.extensions_parameter_overrides to '';
alter system set supautils.extensions_parameter_overrides to '[]';
alter system set supautils.extensions_parameter_overrides to '1';
alter system set supautils.extensions_parameter_overrides to '{"sslinfo": 123}';
alter system set supautils.extensions_parameter_overrides to '{"sslinfo": {"schema": {}}}';
alter system set supautils.extensions_parameter_overrides to '{"sslinfo": {"version": {}}}';
alter system set supautils.extensions_parameter_overrides to '{"sslinfo": {"unknown": "1.0"}}';
alter system set supautils.extensions_parameter_overrides to '{"sslinfo": {"schema": 123}}';
alter system set supautils.extensions_parameter_overrides to '{"sslinfo": {"version": 123}}';
\echo

alter system set supautils.extensions_parameter_overrides to
  '{"sslinfo":{"schema":"pg_catalog"},"hstore":{"version":"1.6"}}';
select pg_reload_conf();
\echo

set role privileged_role;
\echo

-- can force sslinfo to be installed in pg_catalog
create extension sslinfo schema public;
select extnamespace::regnamespace from pg_extension where extname = 'sslinfo';

drop extension sslinfo;
\echo

-- can force hstore to a configured version on create and alter
create extension hstore version '1.7';
select extversion from pg_extension where extname = 'hstore';
alter extension hstore update to '1.7';
select extversion from pg_extension where extname = 'hstore';

drop extension hstore;
\echo

reset role;
alter system set supautils.extensions_parameter_overrides to
  '{"sslinfo":{"schema":"pg_catalog"}}';
select pg_reload_conf();
