-- cannot rename an existing role to a reserved role
alter role fake rename to reserved_but_not_yet_created;
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

-- cannot bypass alter-options check by using current_user
set role anon;
alter role current_user password 'pass';
reset role;
\echo

-- cannot bypass alter-config check by using current_user
set role supabase_storage_admin;
alter role supabase_storage_admin set search_path to 'test';
reset role;
\echo

-- cannot drop a reserved role
drop role fake, supabase_storage_admin;
drop role anon, fake;
\echo

-- cannot create a reserved role that doesn't yet exist
create role reserved_but_not_yet_created;
