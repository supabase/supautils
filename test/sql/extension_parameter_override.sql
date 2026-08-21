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
