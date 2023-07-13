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

-- can manage foreign data wrappers
create extension postgres_fdw;
create foreign data wrapper new_fdw
  handler postgres_fdw_handler
  validator postgres_fdw_validator;

drop extension postgres_fdw cascade;
\echo

-- non-superuser non-privileged role cannot manage bypassrls role attribute
set role rolecreator;
\echo

-- the error message changed in PG14
do $$
begin
  create role r bypassrls;
  exception when insufficient_privilege then null;
end;
$$ language plpgsql;

create role r;
alter role r nobypassrls;
alter role r bypassrls;
drop role r;

set role privileged_role;
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

-- privileged_role can do GRANT <role> to <reserved_role>
grant testme to authenticator;
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

-- member of privileged_role can do privileged role stuff
set role privileged_role_member;

grant testme to authenticator;

set role privileged_role;
