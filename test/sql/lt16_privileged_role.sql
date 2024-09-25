-- non-superuser privileged role
set role privileged_role;
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

-- member of privileged_role can do privileged role stuff
set role privileged_role_member;

grant testme to authenticator;

set role privileged_role;
\echo

-- privileged_role can do GRANT <role> to <reserved_role>
grant testme to authenticator;
\echo
