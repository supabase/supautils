-- supautils settings go through ALTER SYSTEM rather than init.conf so a
-- test can RESET them all the way back to their defaults
alter system set supautils.reserved_roles = 'supabase_storage_admin, anon, reserved_but_not_yet_created, authenticator*';
alter system set supautils.reserved_memberships = 'pg_read_server_files,pg_write_server_files,pg_execute_server_program,role_with_reserved_membership';
alter system set supautils.privileged_extensions = 'autoinc, citext, hstore, sslinfo, insert_username, dict_xsyn, postgres_fdw, pageinspect, plls, no_control_file_extension, dblink, ltree, unaccent, tablefunc, tcn';
alter system set supautils.privileged_role = 'privileged_role';
alter system set supautils.privileged_role_allowed_configs = 'session_replication_role, pgrst.*, other.nested.*';
alter system set supautils.hint_roles = 'hint_role';
alter system set supautils.placeholders = 'response.headers, another.placeholder';
alter system set supautils.placeholders_disallowed_values = '"content-type","x-special-header",special-value';
alter system set supautils.extensions_parameter_overrides = '{"sslinfo":{"schema":"pg_catalog"}}';
alter system set supautils.drop_trigger_grants = '{"privileged_role":["allow_drop_triggers.my_table"]}';
alter system set supautils.policy_grants = '{"privileged_role":["allow_policies_other.other_table","allow_policies.my_table","allow_policies.nonexistent_table"]}';
alter system set supautils.restrict_extension_versions = 'error';
select pg_reload_conf();

-- reserved roles
create role supabase_storage_admin noinherit;
create role anon noinherit;

-- login role
create user rolecreator createrole createdb noinherit nosuperuser;
grant anon to rolecreator with admin option;
grant pg_monitor to rolecreator with admin option;
grant pg_read_all_settings to rolecreator with admin option;
grant pg_read_all_stats to rolecreator with admin option;
grant supabase_storage_admin to rolecreator;

-- other roles
create role fake noinherit;
create role privileged_role login createrole bypassrls replication;
create role privileged_role_member login createrole in role privileged_role;
create role testme noinherit;
grant testme to privileged_role with admin option;
create role authenticator login noinherit;
grant authenticator to privileged_role with admin option;
grant all on database postgres to privileged_role;

-- non-superuser extensions role
create role extensions_role login nosuperuser;
grant all on database contrib_regression, postgres to extensions_role;
alter default privileges for role postgres in schema public grant all on tables to extensions_role;

-- non-superuser that should be unaffected by extension logic when creating db objects
create user nonsuper nosuperuser;
grant all privileges on database contrib_regression to nonsuper;
grant all on schema public to nonsuper;
-- also allow on postgres db for quick manual tests
\c postgres
grant all on schema public to nonsuper;
grant all privileges on database postgres to nonsuper;

-- non-superuser role configured as supautils.superuser for testing version restriction bypass
-- Must be nosuperuser so the test exercises the supautils.superuser name comparison,
-- not the superuser() early return.
create role superuser_for_test nosuperuser login;
grant all on database contrib_regression, postgres to superuser_for_test;

\c contrib_regression

-- starting from pg16 not every role can create objects on the public schema by default
-- this makes the behavior backwards compat
grant all on schema public to privileged_role;
grant all on schema public to rolecreator;
grant all on schema public to supabase_storage_admin;
