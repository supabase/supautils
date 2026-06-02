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
