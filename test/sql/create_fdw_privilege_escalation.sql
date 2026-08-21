-- When creating a foreign data wrapper, supautils allows a user
-- to specify any function as a validator. This gives an attacker a
-- window into running arbitrary code as superuser.

-- Create a new role which the attacker will try to elevate to superuser
create role not_superuser nosuperuser nologin;
set role not_superuser;
\echo 

-- Confirm that the user is not currently superuser
select current_user, rolsuper
from pg_roles
where rolname = current_user;

set role privileged_role;
\echo

-- Create a validator function which elevates privileged_role to superuser
create function public.make_superuser(text[], oid)
returns void
language plpgsql
as
$$
begin
    alter role not_superuser superuser;
end;
$$;
\echo

-- Try to run the exploit
create foreign data wrapper fdw validator public.make_superuser;
\echo

-- Check whether the elevation worked or not
set role not_superuser;
select current_user, rolsuper
from pg_roles
where rolname = current_user;

-- Cleanup
reset role;
drop foreign data wrapper if exists fdw;
drop function if exists public.make_superuser;
\echo

------------------------------------------------------------------
-- Test that a pg_tle based extension owned function can't be
-- used as a validator. This is another vector with a similar escalation
-- path
reset role;
create extension if not exists pg_tle;
grant pgtle_admin to privileged_role;
alter role not_superuser nosuperuser;
set role not_superuser;
\echo 

-- Confirm that the user is not currently superuser
select current_user, rolsuper
from pg_roles
where rolname = current_user;

set role privileged_role;
\echo

select pgtle.install_extension(
    'validator_owning_pg_tle_extension',
    '1.0',
    'A pg_tle extension which creates a validator function',
    $_tle_$
        create or replace function public.pg_tle_make_superuser(options text[], catalog oid)
        returns void
        language plpgsql
        strict
        as $$
        begin
            alter role not_superuser superuser;
        end;
        $$;
    $_tle_$
);

create extension validator_owning_pg_tle_extension;

-- Try to run the exploit
create foreign data wrapper fdw_pg_tle validator public.pg_tle_make_superuser;
\echo

-- Check whether the elevation worked or not
set role not_superuser;
select current_user, rolsuper
from pg_roles
where rolname = current_user;

-- Cleanup
reset role;
revoke pgtle_admin from privileged_role;
drop foreign data wrapper if exists fdw_pg_tle;
drop extension if exists pg_tle cascade;
drop role if exists not_superuser;

------------------------------------------------------------------
-- Test that a search_path based override of a validator function
-- doesn't allow privilege escalation.
reset role;
grant create on database contrib_regression to privileged_role;
create role not_superuser nosuperuser nologin;
set role not_superuser;
\echo

-- Confirm that the user is not currently superuser
select current_user, rolsuper
from pg_roles
where rolname = current_user;

set role privileged_role;
\echo

-- We use postgres_fdw here to move past the provenance check in code that
-- verifies that the validator function belongs to an extesion with a 
-- control file on disk.
create extension postgres_fdw;
\echo

-- Next, we create the validator function named exactly as in the postgres_fdw
-- extension but in a schema named the same as the role supautils elevates
-- to when running the `create foreign data wrapper ...` command.
create schema postgres;
create function postgres.postgres_fdw_validator(text[], oid)
returns void
language plpgsql
as
$$
begin
    alter role not_superuser superuser;
end;
$$;
\echo

-- Finally, the bait-and-switch occurs. The $user variable in search_path
-- resolves to a value of current user. When the code is trying to verify the
-- provenance of the validator function, the role is `privileged_role`.
-- Since no schema named `privileged_role` exists, Postgres moves to the next
-- schema in the search_path: `public`, where it finds the postgres_fdw
-- owned validator function. But once the provenance check passes and 
-- supautils actually runs the `create foreign data wrapper ...` command 
-- as the elevated user `postgres`, Postgres re-resolves the validator
-- function, but this time the $user resolves to `postgres`, running the
-- attacker controlled validator function instead.
set search_path = "$user", public;

-- Try to run the exploit. Reference the validator function unqualified so
-- that the search_path trick works.
create foreign data wrapper fdw_reresolution validator postgres_fdw_validator;
\echo

-- Check whether the elevation worked or not
set role not_superuser;
select current_user, rolsuper
from pg_roles
where rolname = current_user;

-- Cleanup
reset role;
reset search_path;
revoke create on database contrib_regression from privileged_role;
drop foreign data wrapper if exists fdw_reresolution;
drop extension if exists postgres_fdw cascade;
drop schema if exists postgres cascade;
drop role if exists not_superuser;
