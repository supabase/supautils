-- pg_tle lets us define an extension that depends on another one, so that the
-- cascade case can be tested without relying on contrib extensions
do $$
begin
  if not exists (select from pg_extension where extname = 'pg_tle') then
    create extension pg_tle;
  end if;
  if not pg_has_role('extensions_role', 'pgtle_admin', 'member') then
    grant pgtle_admin to extensions_role;
  end if;
end $$;
grant create on schema public to extensions_role;
\echo

set role extensions_role;

select pgtle.install_extension(
  'protected_parent', '1.0', 'required by protected_child',
  $$ create function protected_parent_fn() returns int language sql as 'select 1'; $$
);
select pgtle.install_extension(
  'protected_child', '1.0', 'listed in supautils.protected_extensions',
  $$ create function protected_child_fn() returns int language sql as 'select 2'; $$,
  '{protected_parent}'
);

create extension protected_parent;
create extension protected_child;
\echo

-- the owner cannot drop a protected extension
select extowner::regrole from pg_extension where extname = 'protected_child';
drop extension protected_child;
\echo

-- nor move it to another schema
create schema protected_schema;
alter extension protected_child set schema protected_schema;
\echo

-- nor drop it through an extension it depends on
drop extension protected_parent cascade;
\echo

-- without cascade postgres refuses because of the dependency
drop extension protected_parent;
\echo

-- nor drop it together with other extensions
drop extension protected_parent, protected_child;
\echo

-- both extensions are still there
select extname from pg_extension where extname like 'protected_%' order by 1;
\echo

-- superusers are not restricted
reset role;
drop extension protected_child;
drop extension protected_parent;
select pgtle.uninstall_extension('protected_child');
select pgtle.uninstall_extension('protected_parent');
drop schema protected_schema;
\echo

-- a protected extension that isn't installed is skipped as usual
set role extensions_role;
drop extension if exists protected_child;
