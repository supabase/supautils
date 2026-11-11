-- roles the privileged role has no ADMIN option on, like the ones that were
-- created before a major version upgrade to PG16+
set role postgres;
create role legacy_role;
create role legacy_role_2;
create role legacy_member_of_reserved_membership in role pg_read_server_files;
create role legacy_nested_member_of_reserved_membership in role legacy_member_of_reserved_membership;
create role legacy_member_of_privileged in role privileged_role;
create role legacy_superuser superuser;
create role legacy_member_of_superuser in role legacy_superuser;
create role legacy_member_of_reserved_role in role anon;
\echo

-- non-superuser privileged role
set role privileged_role;
\echo

-- the privileged role cannot manage the roles without the ADMIN option
alter role legacy_role rename to renamed;
drop role legacy_role;
grant legacy_role to fake;
\echo

-- a regular grant to itself is left to core
grant legacy_role to privileged_role;
\echo

-- neither is a GRANTED BY
grant legacy_role to privileged_role with admin option granted by postgres;
\echo

-- cannot give the ADMIN option to a non-privileged role
grant legacy_role to fake with admin option;
grant legacy_role to privileged_role, fake with admin option;
\echo

-- cannot self-grant ADMIN on a superuser or a member of one
grant legacy_superuser to privileged_role with admin option;
grant legacy_member_of_superuser to privileged_role with admin option;
\echo

-- cannot self-grant ADMIN on a reserved role
grant anon to privileged_role with admin option;
\echo

-- cannot self-grant ADMIN on a reserved membership or a member of one
grant pg_read_server_files to privileged_role with admin option;
grant legacy_member_of_reserved_membership to privileged_role with admin option;
grant legacy_nested_member_of_reserved_membership to privileged_role with admin option;
\echo

-- cannot self-grant ADMIN on a predefined role
grant pg_checkpoint to privileged_role with admin option;
\echo

-- cannot self-grant ADMIN on a member of the privileged role
grant legacy_member_of_privileged to privileged_role with admin option;
\echo

-- one disallowed role makes the whole statement be left to core
grant legacy_role, legacy_superuser to privileged_role with admin option;
\echo

-- ensure the hook doesn't interfere with regular error messages
grant nonexistent to privileged_role with admin option;
grant legacy_role to nonexistent with admin option;
grant legacy_role to public with admin option;
\echo

-- non-privileged role with CREATEROLE cannot self-grant ADMIN
set role rolecreator;
grant legacy_role to rolecreator with admin option;
set role privileged_role;
\echo

-- can self-grant ADMIN
grant legacy_role to privileged_role with admin option;
\echo

-- can self-grant ADMIN on many roles, including members of reserved roles
grant legacy_role_2, legacy_member_of_reserved_role to current_user with admin true;
\echo

-- the grants are the same as the ones for a role created on PG16+
select roleid::regrole, member::regrole, grantor::regrole, admin_option
from pg_auth_members
where roleid::regrole::text like 'legacy_%' and member = 'privileged_role'::regrole
order by 1;
\echo

-- now the privileged role can manage the roles
grant legacy_role to fake;
revoke legacy_role from fake;
alter role legacy_role rename to renamed;
alter role renamed rename to legacy_role;
\echo

-- works inside a function, where the statement is cached
set role postgres;
create role legacy_role_4;
set role privileged_role;
create function self_grant_admin() returns void language plpgsql as $$
begin
  grant legacy_role_4 to current_user with admin option;
end $$;
select self_grant_admin();
\echo

-- a member of the privileged role can do it too
set role privileged_role_member;
create role legacy_role_3;
set role postgres;
revoke legacy_role_3 from privileged_role_member;
set role privileged_role_member;
grant legacy_role_3 to privileged_role_member with admin option;
drop role legacy_role_3;
set role privileged_role;
\echo

-- already having ADMIN keeps the privileged role as the grantor
grant legacy_role to privileged_role_member with admin option;
select roleid::regrole, member::regrole, grantor::regrole, admin_option
from pg_auth_members
where roleid = 'legacy_role'::regrole and member = 'privileged_role_member'::regrole;
\echo

-- cleanup
drop function self_grant_admin();
drop role legacy_role;
drop role legacy_role_2;
drop role legacy_role_4;
drop role legacy_member_of_reserved_role;
set role postgres;
drop role legacy_nested_member_of_reserved_membership;
drop role legacy_member_of_reserved_membership;
drop role legacy_member_of_privileged;
drop role legacy_member_of_superuser;
drop role legacy_superuser;
set role privileged_role;
