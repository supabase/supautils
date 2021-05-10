-- can't create a reserved role
create role reserved_but_not_yet_created;
-- can't create a reserved role, even if it exists
create role anon;
-- cannot rename an existing role to a reserved role
alter role fake rename to reserved_but_not_yet_created;
-- cannot rename a reserved role
alter role supabase_storage_admin rename to another;
-- cannot alter-options a reserved role
alter role supabase_storage_admin nologin superuser;
alter role supabase_storage_admin password 'pass';
-- cannot drop a reserved role
drop role fake, supabase_storage_admin;
drop role anon, fake;
