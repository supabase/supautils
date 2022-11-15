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

-- non-superuser non-privileged role cannot manage bypassrls role attribute
set role rolecreator;

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
