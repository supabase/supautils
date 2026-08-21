-- check json validation works
alter system set supautils.drop_trigger_grants to '';
alter system set supautils.drop_trigger_grants to '[]';
alter system set supautils.drop_trigger_grants to '1';
alter system set supautils.drop_trigger_grants to '"foo"';
alter system set supautils.drop_trigger_grants to '{"my_role": {}}';
alter system set supautils.drop_trigger_grants to '{"my_role": 1}';
alter system set supautils.drop_trigger_grants to '{"my_role": "foo"}';
\echo

-- check valid values are accepted
alter system set supautils.drop_trigger_grants to '{}';
alter system set supautils.drop_trigger_grants to '{ "my_role": [] }';
alter system set supautils.drop_trigger_grants to '{ "my_role": ["public.not_my_table", "public.also_not_my_table"] }';
\echo

-- Upto 100 grants can be set successfully
select '{' || string_agg(format('"ext_%s":[]', i), ',') || '}' as grants
from generate_series(1, 100) i \gset
\echo

alter system set supautils.drop_trigger_grants to :'grants';
\echo

-- More than 100 raise an error instead
select '{' || string_agg(format('"ext_%s":[]', i), ',') || '}' as grants
from generate_series(1, 101) i \gset
\echo

alter system set supautils.drop_trigger_grants to :'grants';
\echo

-- Upto 100 table grants can be set successfully
select '{"ext":[' || string_agg(format('"public.table_%s"', i), ',') || ']}' as grants
from generate_series(1, 100) i \gset
\echo

alter system set supautils.drop_trigger_grants to :'grants';
\echo

-- More than 100 raise an error instead
select '{"ext":[' || string_agg(format('"public.table_%s"', i), ',') || ']}' as grants
from generate_series(1, 101) i \gset
\echo

alter system set supautils.drop_trigger_grants to :'grants';
\echo
