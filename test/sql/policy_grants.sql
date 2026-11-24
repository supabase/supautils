-- check json validation works
alter system set supautils.policy_grants to '';
alter system set supautils.policy_grants to '[]';
alter system set supautils.policy_grants to '1';
alter system set supautils.policy_grants to '"foo"';
alter system set supautils.policy_grants to '{"my_role": {}}';
alter system set supautils.policy_grants to '{"my_role": 1}';
alter system set supautils.policy_grants to '{"my_role": "foo"}';
\echo

-- check valid values are accepted
alter system set supautils.policy_grants to '{}';
alter system set supautils.policy_grants to '{ "my_role": [] }';
alter system set supautils.policy_grants to '{ "my_role": ["public.not_my_table", "public.also_not_my_table"] }';
\echo

-- Upto 100 grants can be set successfully
select '{' || string_agg(format('"ext_%s":[]', i), ',') || '}' as grants
from generate_series(1, 100) i \gset
\echo

alter system set supautils.policy_grants to :'grants';
\echo

-- More than 100 raise an error instead
select '{' || string_agg(format('"ext_%s":[]', i), ',') || '}' as grants
from generate_series(1, 101) i \gset
\echo

alter system set supautils.policy_grants to :'grants';
\echo

-- Upto 100 table grants can be set successfully
select '{"ext":[' || string_agg(format('"public.table_%s"', i), ',') || ']}' as grants
from generate_series(1, 100) i \gset
\echo

alter system set supautils.policy_grants to :'grants';
\echo

-- More than 100 raise an error instead
select '{"ext":[' || string_agg(format('"public.table_%s"', i), ',') || ']}' as grants
from generate_series(1, 101) i \gset
\echo

alter system set supautils.policy_grants to :'grants';
\echo

-- alter system runs the check hook in this backend but never the assign hook,
-- so a rejected value must leave the grants loaded at startup untouched:
-- privileged_role keeps its allow_policies.my_table grant from init.conf
create schema allow_policies;
create table allow_policies.my_table ();
grant usage on schema allow_policies to privileged_role;
alter system set supautils.policy_grants to '{"privileged_role": 1}';
set role privileged_role;
create policy p on allow_policies.my_table for select using (true);
drop policy p on allow_policies.my_table;
reset role;
drop schema allow_policies cascade;
\echo

-- a value rejected halfway (grants_role parsed, oops fails) must not leave
-- partial entries behind for a later accepted value to build on.
-- no reload on purpose: alter system writes the file and runs only the check
-- hook, so grants_role must still have no grants and the policy on my_table
-- is denied
create schema allow_policies;
create table allow_policies.my_table ();
create table allow_policies.other_table ();
create role grants_role;
grant usage on schema allow_policies to grants_role;
alter system set supautils.policy_grants to '{"grants_role": ["allow_policies.my_table"], "oops": 1}';
alter system set supautils.policy_grants to '{"grants_role": ["allow_policies.other_table"]}';
set role grants_role;
create policy p on allow_policies.my_table for select using (true);
reset role;
drop table allow_policies.my_table, allow_policies.other_table;
drop schema allow_policies;
drop role grants_role;
alter system reset supautils.policy_grants;
\echo

-- a slot left half parsed by a rejected value must not keep its table count:
-- after a value with too many tables is rejected, a valid value for the same
-- role must be accepted
select '{"stale_role":[' || string_agg(format('"public.table_%s"', i), ',') || ']}' as grants
from generate_series(1, 101) i \gset
\echo

alter system set supautils.policy_grants to :'grants';
alter system set supautils.policy_grants to '{"stale_role": ["public.table_1"]}';
alter system reset supautils.policy_grants;
\echo
