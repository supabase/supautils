-- constrained by cpu
create extension adminpack;
\echo

-- constrained by memory
create extension cube;
\echo

-- constrained by disk
create extension lo;
\echo

-- passes all resource constraints
create extension amcheck;
\echo

-- no resource constraints
create extension bloom;
\echo

-- check json validation works
alter system set supautils.constrained_extensions to '';
alter system set supautils.constrained_extensions to '[]';
alter system set supautils.constrained_extensions to '1';
alter system set supautils.constrained_extensions to '"foo"';
alter system set supautils.constrained_extensions to '{"plrust": []}';
alter system set supautils.constrained_extensions to '{"plrust": {"cpu": true}}';
alter system set supautils.constrained_extensions to '{"plrust": {"cpu": {}}}';
alter system set supautils.constrained_extensions to '{"plrust": {"anykey": "11GB"}}';
alter system set supautils.constrained_extensions to '{"plrust": {"disk": 123}}';
alter system set supautils.constrained_extensions to '{"plrust": {"mem": 456}}';
alter system set supautils.constrained_extensions to '{"plrust": {"mem": ""}}';
alter system set supautils.constrained_extensions to '{"plrust": 123}';
\echo

-- check valid values are accepted
alter system set supautils.constrained_extensions to '{"plrust": {"cpu": 16, "mem": "1 GB", "disk": "500 MB"}, "pg_net": { "mem": "1 GB"}, "pg_tle": {}}';
\echo

-- Upto 100 constraints can be set successfully
select '{' || string_agg(format('"ext_%s":{}', i), ',') || '}' as constraints
from generate_series(1, 100) i \gset
\echo

alter system set supautils.constrained_extensions to :'constraints';
\echo

-- More than 100 raise an error instead
select '{' || string_agg(format('"ext_%s":{}', i), ',') || '}' as constraints
from generate_series(1, 101) i \gset
\echo

alter system set supautils.constrained_extensions to :'constraints';
\echo