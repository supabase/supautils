-- check json validation works
alter system set supautils.extensions_parameter_overrides to '';
alter system set supautils.extensions_parameter_overrides to '[]';
alter system set supautils.extensions_parameter_overrides to '1';
alter system set supautils.extensions_parameter_overrides to '{"sslinfo": 123}';
alter system set supautils.extensions_parameter_overrides to '{"sslinfo": {"schema": {}}}';
alter system set supautils.extensions_parameter_overrides to '{"sslinfo": {"version": "1.0"}}';
alter system set supautils.extensions_parameter_overrides to '{"sslinfo": {"schema": 123}}';
\echo

-- can force sslinfo to be installed in pg_catalog
create extension sslinfo schema public;
select extnamespace::regnamespace from pg_extension where extname = 'sslinfo';

drop extension sslinfo;
