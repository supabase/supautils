-- ALTER ROLE ALL names no single role, so there is nothing for the role policy
-- to check. The statements must fall through to core's own permission checks.
set role nonsuper;
\echo

alter role all set statement_timeout = '1s';
alter user all set statement_timeout = '1s';
alter role all in database contrib_regression set statement_timeout = '1s';
alter role all reset statement_timeout;
alter role all reset all;
\echo

-- privileged_role is not a superuser either
set role privileged_role;
\echo

alter role all set statement_timeout = '1s';
alter role all in database contrib_regression reset all;
alter role all reset all;
\echo

-- public is not a role; core raises its usual error
alter role public set statement_timeout = '1s';
alter role public nologin;
\echo

reset role;
-- the session is still usable
select 1 as alive;
