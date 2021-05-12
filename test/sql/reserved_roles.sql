-- cannot rename an existing role to a reserved role
alter role fake rename to reserved_but_not_yet_created;

-- cannot rename a reserved role
alter role supabase_storage_admin rename to another;

-- cannot alter-options a reserved role
alter role supabase_storage_admin nologin superuser;
alter role supabase_storage_admin password 'pass';

-- cannot alter-config for a reserved role
alter role supabase_storage_admin set search_path to 'test';

-- cannot bypass alter-options check by using current_user
set role anon;
alter role current_user password 'pass';
reset role;

-- cannot bypass alter-config check by using current_user
set role supabase_storage_admin;
alter role supabase_storage_admin set search_path to 'test';
reset role;

-- cannot drop a reserved role
drop role fake, supabase_storage_admin;
drop role anon, fake;

-- cannot create a reserved role
create role reserved_but_not_yet_created;

-- cannot create a reserved role, even if it exists
create role anon;
