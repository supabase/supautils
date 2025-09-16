-- non-superuser privileged role
set role privileged_role;
\echo

-- can set privileged_role_allowed_configs
set session_replication_role to 'replica';
begin;
  set local session_replication_role to 'origin';
commit;
reset session_replication_role;
\echo

-- non-superuser non-privileged role cannot set privileged_role_allowed_configs
set role rolecreator;

set session_replication_role to 'replica';
begin;
  set local session_replication_role to 'origin';
commit;
reset session_replication_role;

set role privileged_role;
\echo

-- superuser can set privileged_role_allowed_configs
set role postgres;

set session_replication_role to 'replica';
begin;
  set local session_replication_role to 'origin';
commit;
reset session_replication_role;

set role privileged_role;
\echo

-- can set privileged_role_allowed_configs for a role
create role r;
alter role r set session_replication_role to 'replica';
drop role r;
\echo

-- can manage bypassrls role attribute
create role r bypassrls;
select rolbypassrls from pg_roles where rolname = 'r';
alter role r nobypassrls;
select rolbypassrls from pg_roles where rolname = 'r';
alter role r bypassrls;
select rolbypassrls from pg_roles where rolname = 'r';
drop role r;
\echo

-- can manage replication role attribute
create role r replication;
select rolreplication from pg_roles where rolname = 'r';
alter role r noreplication;
select rolreplication from pg_roles where rolname = 'r';
alter role r replication;
select rolreplication from pg_roles where rolname = 'r';

drop role r;
\echo

-- can manage foreign data wrappers
create extension postgres_fdw;
create foreign data wrapper new_fdw
  handler postgres_fdw_handler
  validator postgres_fdw_validator;

drop extension postgres_fdw cascade;
\echo

-- superuser can manage bypassrls role attribute
set role postgres;

create role r bypassrls;
select rolbypassrls from pg_roles where rolname = 'r';
alter role r nobypassrls;
select rolbypassrls from pg_roles where rolname = 'r';
alter role r bypassrls;
select rolbypassrls from pg_roles where rolname = 'r';
drop role r;

set role privileged_role;
\echo

-- regression: https://github.com/supabase/supautils/issues/34
create role tmp;
alter role tmp;
\echo

-- privileged_role can modify reserved roles GUCs
set role privileged_role;
alter role authenticator set search_path to public;
alter role authenticator set statement_timeout = '15s';
\echo

-- privileged_role can set wildcard privileged_role_allowed_configs
alter role authenticator set pgrst.db_plan_enabled to true;
alter role authenticator set pgrst.db_prepared_statements to false;
alter role authenticator set other.nested.foo to true;
alter role authenticator set other.nested.bar to true;
\echo

-- privileged_role cannot drop, rename or nologin reserved role
drop role authenticator;
alter role authenticator rename to authorized;
alter role authenticator nologin;
\echo

-- privileged_role can create nosuperuser
create role r nosuperuser;

drop role r;
\echo

-- privileged_role cannot create superuser or alter [no]superuser
create role r superuser;
create role r;
alter role r superuser;
alter role r nosuperuser;

drop role r;
\echo

-- privileged_role can manage publications
create publication p for all tables;
drop publication p;
-- not testing `create publication ... for tables in schema ...` because it's PG15+
\echo

-- privileged_role can manage policies on tables in allowlist
set role postgres;
create schema allow_policies;
create table allow_policies.my_table ();
grant usage on schema allow_policies to privileged_role;
set role privileged_role;
create policy p on allow_policies.my_table for select using (true);
alter policy p on allow_policies.my_table using (false);
drop policy p on allow_policies.my_table;

set role postgres;
drop schema allow_policies cascade;
set role privileged_role;
\echo

-- privileged_role cannot manage policies on tables not in allowlist
set role postgres;
create schema deny_policies;
create table deny_policies.my_table ();
create policy p1 on deny_policies.my_table for select using (true);
grant usage on schema deny_policies to privileged_role;
set role privileged_role;
create policy p2 on deny_policies.my_table for select using (true);
alter policy p1 on deny_policies.my_table using (false);
drop policy p1 on deny_policies.my_table;

set role postgres;
drop schema deny_policies cascade;
set role privileged_role;
\echo

-- privileged_role can drop triggers on tables in allowlist
set role postgres;
create schema allow_drop_triggers;
create table allow_drop_triggers.my_table ();
create function allow_drop_triggers.f() returns trigger as 'begin return null; end' language plpgsql;
create trigger tr after insert on allow_drop_triggers.my_table execute function allow_drop_triggers.f();
grant usage on schema allow_drop_triggers to privileged_role;
set role privileged_role;
drop trigger tr on allow_drop_triggers.my_table;

set role postgres;
drop schema allow_drop_triggers cascade;
set role privileged_role;
\echo

-- privileged_role cannot drop triggers on tables not in allowlist
set role postgres;
create schema deny_drop_triggers;
create table deny_drop_triggers.my_table ();
create function deny_drop_triggers.f() returns trigger as 'begin return null; end' language plpgsql;
create trigger tr after insert on deny_drop_triggers.my_table execute function deny_drop_triggers.f();
grant usage on schema deny_drop_triggers to privileged_role;
set role privileged_role;
drop trigger tr on deny_drop_triggers.my_table;

set role postgres;
drop schema deny_drop_triggers cascade;
set role privileged_role;
\echo

-- all-role including superuser cannot drop information_schema
set role postgres;
create schema allow_drop_schema;
drop schema information_schema;
drop schema information_schema cascade;
drop schema allow_drop_schema, information_schema;
drop schema allow_drop_schema;
\echo
