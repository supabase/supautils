-- With every supautils setting at its default the hook must leave every
-- statement to core. init.sql sets them through ALTER SYSTEM so RESET can
-- reach the default.
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
\c
select source = 'default' as unset from pg_settings where name = 'supautils.reserved_roles';

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
\c
select source = 'configuration file' as restored from pg_settings where name = 'supautils.reserved_roles';
drop table settings_unset_t;
