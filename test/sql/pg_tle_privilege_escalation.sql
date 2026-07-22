-- This test verfies that a privilege escalation attack vector is closed. First, let's
-- understand how the attack worked before the fix:
-- 
-- 1. supautils.privileged_extensions has an extension listed in it which does not
--    have a control file. For example `plls` which is not available on many projects
--    on our platform.
-- 2. A non-superuser attacker runs the following code:
--
--    create extension if not exists pg_tle; 
--    select pgtle.install_extension(
--        'plls',
--        '1.0',
--        'Shadow plls which is not available, hence no control file',
--        $$
--            create table if not exists public.user as
--                select current_user as username,
--                (select rolsuper from pg_roles where rolname=current_user) as is_superuser;
--        $$
--    );
--
--    The above code registers an extension named `plls` as a pg_tle extension with the
--    code to create the poc_privsec table and insert the current_user and it's superuser
--    status as columns.
--
-- 3. The attacker then runs `create extension plls version '1.0';` which is first
--    caught by the supautils hook, which looks in the supautils.privileged_extensions
--    GUC for the presence of `plls`, finds it and elevates the role to superuser.
--    Next, the pg_tle hook runs, finds that plls is not a control file based extension
--    and runs the code registered against the plls extension as superuser. This can be
--    verified by running the following code:
--
--    select * from public.user;
--
--    Which shows that the extension code indeed ran as a superuser:
--
--     username | is_superuser 
--    ----------+--------------
--     postgres | t
--
-- The fix was to filter out control-file-less extensions from the
-- supautils.privileged_extensions GUC, leaving behind only extensions with a controle file.
-- Since pg_tle does not shadowing an extension with control file, the attacker wouldn't be
-- able to register the sql with pgtle.install_extension call, which fails with an error:
--
-- Error: Failed to run sql query: ERROR: 55000: control file already exists for the
-- address_standardizer extension
--
-- Thus closing the attack vector.

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
    'Shadow no_control_file_extension which is has no control file on disk',
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
