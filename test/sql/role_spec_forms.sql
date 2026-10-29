-- ALTER ROLE ALL names no single role, so there is nothing for the role policy
-- to check; the statements must fall through to core's own permission checks.
-- The IN DATABASE form is not exercised anywhere else.
set role nonsuper;
alter role all set statement_timeout = '1s';
alter user all set statement_timeout = '1s';
alter role all in database contrib_regression set statement_timeout = '1s';
alter role all reset statement_timeout;
alter role all reset all;
alter role nonsuper in database contrib_regression reset statement_timeout;
alter role anon in database contrib_regression set statement_timeout = '1s';

-- privileged_role is not a superuser either
set role privileged_role;
alter role all set statement_timeout = '1s';
alter role all in database contrib_regression reset all;
alter role all reset all;
alter role anon in database contrib_regression set statement_timeout = '1s';

reset role;
-- the session is still usable
select 1 as alive;
