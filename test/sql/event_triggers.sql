create or replace function show_current_user()
    returns event_trigger
    language plpgsql as
$$
begin
    raise notice 'the event trigger is executed for %', current_user;
end;
$$;

grant all on schema public to privileged_role;
grant all on schema public to rolecreator;
grant all on schema public to supabase_storage_admin;

set role rolecreator;
\echo

-- A role other than privileged_role shouldn't be able to create the event trigger
create event trigger event_trigger_1 on ddl_command_end
execute procedure show_current_user();
\echo

set role privileged_role;
\echo

-- The privileged_role should be able to create the event trigger
create event trigger event_trigger_1 on ddl_command_end
execute procedure show_current_user();
\echo

-- The privileged_role should execute the event trigger function
create table privileged_stuff();
\echo

set role rolecreator;
\echo

-- A role other than privileged_role should execute the event trigger function
create function dummy() returns text as $$ select 'dummy'; $$ language sql;
\echo

set role supabase_storage_admin;
\echo

-- A reserved_role shouldn't execute the event trigger function
create table storage_stuff();
\echo

drop table storage_stuff;
\echo

set role postgres;
\echo

-- A superuser role shouldn't execute the event trigger function
create table super_stuff();
\echo

drop event trigger event_trigger_1;
revoke all on schema public from privileged_role;
revoke all on schema public from rolecreator;
revoke all on schema public from supabase_storage_admin;
