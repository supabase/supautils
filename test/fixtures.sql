-- reserved roles
create role supabase_storage_admin noinherit;
create role anon noinherit;

-- login role
create user rolecreator createrole createdb noinherit nosuperuser;
grant anon to rolecreator with admin option;
grant pg_monitor to rolecreator with admin option;
grant pg_read_all_settings to rolecreator with admin option;
grant pg_read_all_stats to rolecreator with admin option;
grant supabase_storage_admin to rolecreator;

-- other roles
create role fake noinherit;
create role privileged_role login createrole bypassrls replication;
create role privileged_role_member login createrole in role privileged_role;
create role testme noinherit;
grant testme to privileged_role with admin option;
create role authenticator login noinherit;
grant authenticator to privileged_role with admin option;
grant all on database postgres to privileged_role;

-- non-superuser extensions role
create role extensions_role login nosuperuser;
grant all on database postgres to extensions_role;
alter default privileges for role postgres in schema public grant all on tables to extensions_role;
