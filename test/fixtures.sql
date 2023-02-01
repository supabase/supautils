-- reserved roles
create role supabase_storage_admin noinherit;
create role anon noinherit;

-- login role
create user rolecreator createrole noinherit nosuperuser;
grant anon to rolecreator;
grant supabase_storage_admin to rolecreator;

-- other roles
create role fake noinherit;
create role privileged_role login createrole;
create role testme noinherit;
create role authenticator login noinherit;
