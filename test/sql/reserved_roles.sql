-- use a role that has the CREATEROLE privilege
set role rolecreator;
\echo

-- cannot rename an existing role to a reserved role
create role r;
alter role r rename to reserved_but_not_yet_created;

drop role r;
\echo

-- cannot rename a reserved role
alter role supabase_storage_admin rename to another;
\echo

-- cannot alter-options a reserved role
alter role supabase_storage_admin nologin superuser;
alter role supabase_storage_admin password 'pass';
\echo

-- cannot alter-config for a reserved role
alter role supabase_storage_admin set search_path to 'test';
\echo

-- cannot drop a reserved role
drop role fake, supabase_storage_admin;
drop role anon, fake;
\echo

-- ensure the hook doesn't interfere with regular error messages
drop role public;
drop role current_user;
drop role session_user;
alter role public;
drop role if exists nonexistent, rol;
alter table foo rename to bar;
\echo

-- cannot create a reserved role that doesn't yet exist
create role reserved_but_not_yet_created;
\echo

-- since anon already exists, bypass the hook and show normal "already exists" error
create role anon;

-- ensure our hooks don't mess regular non-reserved roles functionality
create role r;
alter role r rename to new_r;
alter role new_r createrole createdb;
drop role new_r;
create role non_reserved;
\echo

-- cannnot grant memberships to reserved-roles
grant pg_monitor to anon;
grant pg_read_all_settings to anon;
grant pg_read_all_stats to anon;
\echo

-- cannnot revoke memberships from reserved-roles
revoke pg_monitor from anon;
revoke pg_read_all_settings from anon;
revoke pg_read_all_stats from anon;
\echo

-- cannot bypass alter-options check by using current_user
set role anon;
alter role current_user password 'pass';
\echo

-- cannot bypass alter-config check by using current_user
set role supabase_storage_admin;
alter role current_user set search_path to 'test';
\echo

-- use a role that has the SUPERUSER privilege
set role postgres;
\echo

-- SUPERUSER can do anything with reserved roles
create role reserved_but_not_yet_created;
alter role reserved_but_not_yet_created set search_path to 'test';
alter role reserved_but_not_yet_created login bypassrls;
alter role reserved_but_not_yet_created rename to renamed_reserved_role;
drop owned by supabase_storage_admin cascade;
drop role supabase_storage_admin;
