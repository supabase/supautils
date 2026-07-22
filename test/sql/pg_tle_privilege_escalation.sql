-- This test verfies that a privilege escalation attack vector is closed. First, let's
-- understand how the attack worked before the fix:
-- 
-- 1. supautils.privileged_extensions had an extension listed in it which did not
--    have a control file.
-- 2. A non-superuser attacker ran the following code:
--
--    create extension if not exists pg_tle; 
--    select pgtle.install_extension(
--        'no_control_file_extension',
--        '1.0',
--        'Shadow no_control_file_extension which has no control file on disk',
--        $$
--            create table if not exists public.user as
--                select current_user as username,
--                (select rolsuper from pg_roles where rolname=current_user) as is_superuser;
--        $$
--    );
--
--    The above code registered an extension named `no_control_file_extension` as a pg_tle extension
--    with the code to create the public.user table and insert the current_user and its superuser
--    status as columns.
--
-- 3. The attacker then ran `create extension no_control_file_extension version '1.0';` which was first
--    caught by the supautils hook, which looks in the supautils.privileged_extensions GUC for the
--    presence of `no_control_file_extension`, finds it and elevates the role to superuser.
--    Next, the pg_tle hook runs, finds that no_control_file_extension is not a control file based
--    extension and runs the code registered against the no_control_file_extension extension as superuser.
--    This can be verified by running the following code:
--
--    select * from public.user;
--
--    Which shows that the extension code indeed ran as a superuser:
--
--     username | is_superuser 
--    ----------+--------------
--     postgres | t
--
-- The fix is to look at pg_catalog.pg_available_extensions to find out extensions with
-- a control file on disk and only escalate to superuser if an extension is found in this
-- view.
--
-- Actual test follows.

-- off mode allows specifying versions which this test needs
set supautils.restrict_extension_versions = off;
\echo

-- Needs superuser to create pg_tle and setup permissions
create extension if not exists pg_tle;
grant pgtle_admin to extensions_role;
grant create on schema public to extensions_role;
\echo

-- Now we use the non-superuser extensions_role to drop the privileges for the rest of the test
set role extensions_role;
\echo

--Verify that pg_tle does not allow shadowing an extension which already has a control file on disk
select pgtle.install_extension(
    'pageinspect',
    '1.0',
    'Trying to shadow pageinspect which already has a control file on disk',
    $$
        select 1;
    $$
);
\echo

-- Wrap the creation of the pageinspect extension in begin/rollback pair
-- to avoid polluting results for other tests.
begin;
-- But even with pg_tle installed, extensions with on disk control file are still run as superuser
create extension pageinspect;
\echo

-- Verify that pageinspect was created by the superuser postgres, not extensions_role
select rolname as extension_owner
from pg_extension e
join pg_roles r on r.oid = e.extowner
where e.extname = 'pageinspect';
rollback;
\echo

-- Now test the privilege escalation attempt by letting a TLE extension claim an extension
-- which has no control file on disk. This should not escalate to superuser.
select pgtle.install_extension(
    'no_control_file_extension',
    '1.0',
    'Shadow no_control_file_extension which has no control file on disk',
    $$
        create table if not exists public.user as
            select current_user as username,
            (select rolsuper from pg_roles where rolname=current_user) as is_superuser;
    $$
);
\echo

-- Create the extension and verify that the attempt to escalate the privilege failed
create extension no_control_file_extension version '1.0';
select * from public.user;

-- A similar test for alter extension update command
truncate table public.user;

-- Register an upgrade path for the extension
select pgtle.install_update_path(
    'no_control_file_extension',
    '1.0',
    '2.0',
    $$
        insert into public.user (username, is_superuser)
        select current_user, rolsuper 
        from pg_roles 
        where rolname = current_user;
    $$
);
\echo

-- Update the extension and verify that the attempt to escalate the privilege failed
alter extension no_control_file_extension update to '2.0';
select * from public.user;
