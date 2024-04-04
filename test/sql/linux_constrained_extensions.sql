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
set supautils.constrained_extensions to '';
set supautils.constrained_extensions to '[]';
set supautils.constrained_extensions to '1';
set supautils.constrained_extensions to '"foo"';
set supautils.constrained_extensions to '{"plrust": []}';
set supautils.constrained_extensions to '{"plrust": {"cpu": true}}';
set supautils.constrained_extensions to '{"plrust": {"cpu": {}}}';
set supautils.constrained_extensions to '{"plrust": {"anykey": "11GB"}}';
set supautils.constrained_extensions to '{"plrust": {"disk": 123}}';
set supautils.constrained_extensions to '{"plrust": {"mem": 456}}';
set supautils.constrained_extensions to '{"plrust": {"mem": ""}}';
set supautils.constrained_extensions to '{"plrust": 123}';
