-- what this test is about:
-- supautils only acts when its settings are configured. with nothing
-- configured every hook should fall through and let core handle the
-- statement, this test makes sure of that.
--
-- how it works:
-- 1. save the current values of the settings with \gset so they can be
--    restored at the end
-- 2. alter system reset each one so they go back to their defaults, reload
--    and reconnect so the new session sees the defaults
-- 3. run the statements the hooks intercept (alter role, create role, grant,
--    create extension, create policy, set, create publication) as nonsuper
--    and then as privileged_role. the errors in the output should be the
--    ones core gives, no supautils error or hint should show up
-- 4. put the settings back and reconnect so the next test runs with the
--    usual config
create table settings_unset_t (id int);
select current_setting('supautils.reserved_roles') as reserved_roles, current_setting('supautils.reserved_memberships') as reserved_memberships, current_setting('supautils.privileged_role') as privileged_role, current_setting('supautils.privileged_role_allowed_configs') as privileged_role_allowed_configs, current_setting('supautils.privileged_extensions') as privileged_extensions, current_setting('supautils.policy_grants') as policy_grants, current_setting('supautils.drop_trigger_grants') as drop_trigger_grants, current_setting('supautils.restrict_extension_versions') as restrict_extension_versions \gset
alter system reset supautils.reserved_roles;
alter system reset supautils.reserved_memberships;
alter system reset supautils.privileged_role;
alter system reset supautils.privileged_role_allowed_configs;
alter system reset supautils.privileged_extensions;
alter system reset supautils.policy_grants;
alter system reset supautils.drop_trigger_grants;
alter system reset supautils.restrict_extension_versions;
select pg_reload_conf();
-- reconnect, pg_reload_conf only signals the backends and the change
-- may not be picked up before the next statement runs
\c
-- every setting should now come from its default
select name, source
from pg_settings
where name in (
    'supautils.reserved_roles',
    'supautils.reserved_memberships',
    'supautils.privileged_role',
    'supautils.privileged_role_allowed_configs',
    'supautils.privileged_extensions',
    'supautils.policy_grants',
    'supautils.drop_trigger_grants',
    'supautils.restrict_extension_versions')
order by name;

set role nonsuper;
alter role anon nologin;
alter role anon set statement_timeout = '1s';
create role settings_unset_member in role anon;
grant pg_read_server_files to nonsuper;
create extension postgres_fdw;
create policy p on settings_unset_t using (true);
set session_replication_role = 'replica';
create publication settings_unset_pub for table settings_unset_t;
reset role;

set role privileged_role;
alter role anon nologin;
alter role anon set statement_timeout = '1s';
create role settings_unset_member in role anon;
grant pg_read_server_files to privileged_role;
create extension postgres_fdw;
create policy p on settings_unset_t using (true);
set session_replication_role = 'replica';
create publication settings_unset_pub for table settings_unset_t;
reset role;

alter system set supautils.reserved_roles = :'reserved_roles';
alter system set supautils.reserved_memberships = :'reserved_memberships';
alter system set supautils.privileged_role = :'privileged_role';
alter system set supautils.privileged_role_allowed_configs = :'privileged_role_allowed_configs';
alter system set supautils.privileged_extensions = :'privileged_extensions';
alter system set supautils.policy_grants = :'policy_grants';
alter system set supautils.drop_trigger_grants = :'drop_trigger_grants';
alter system set supautils.restrict_extension_versions = :'restrict_extension_versions';
select pg_reload_conf();
-- reconnect so the restored values are picked up
\c
-- and back from the config file after the restore
select name, source
from pg_settings
where name in (
    'supautils.reserved_roles',
    'supautils.reserved_memberships',
    'supautils.privileged_role',
    'supautils.privileged_role_allowed_configs',
    'supautils.privileged_extensions',
    'supautils.policy_grants',
    'supautils.drop_trigger_grants',
    'supautils.restrict_extension_versions')
order by name;
drop table settings_unset_t;
