-- create a function owned by a non-superuser
set role privileged_role;
\echo

create function show_current_user()
    returns event_trigger
    language plpgsql as
$$
begin
    raise notice 'the event trigger is executed for %', current_user;
end;
$$;
\echo

-- A role other than privileged_role shouldn't be able to create an event trigger
set role rolecreator;
\echo

create event trigger event_trigger_1 on ddl_command_end
execute procedure show_current_user();
\echo

-- The privileged_role should be able to create the event trigger
set role privileged_role;
\echo

create event trigger event_trigger_1 on ddl_command_end
execute procedure show_current_user();
\echo

-- the event trigger is owned by the privileged_role
select evtowner::regrole from pg_event_trigger where evtname = 'event_trigger_1';

-- The privileged_role should execute the event trigger function
create table privileged_stuff();
\echo

-- A superuser is not able to create event trigger using non-superuser function
set role postgres;
\echo

create event trigger event_trigger_2 on ddl_command_end
execute procedure show_current_user();
\echo

-- A role other than privileged_role should execute the event trigger function
set role rolecreator;
\echo

create function dummy() returns text as $$ select 'dummy'; $$ language sql;
\echo

-- A reserved_role shouldn't execute the event trigger function
set role supabase_storage_admin;
\echo

create table storage_stuff();
\echo

drop table storage_stuff;
\echo

-- A superuser role shouldn't execute the event trigger function
set role postgres;
\echo

create table super_stuff();
\echo

-- extensions won't execute the event trigger function (since they're executed by superuser under our implementation)
set role rolecreator;
\echo

create extension postgres_fdw;
drop extension postgres_fdw;
\echo

-- privesc shouldn't happen due to superuser tripping over a user-defined event trigger
set role privileged_role;
\echo

create or replace function become_super()
    returns event_trigger
    language plpgsql as
$$
begin
    raise notice 'transforming % to superuser', current_user;
    alter role rolecreator superuser;
end;
$$;
\echo

create event trigger event_trigger_2 on ddl_command_end
execute procedure become_super();
\echo

-- when switching to super, the become_super evtrig won't fire
set role postgres;
\echo

create table super_duper_stuff();
select count(*) = 1 as only_one_super from pg_roles where rolsuper;

-- ensure logging skipped event triggers happens when enabled
set supautils.log_skipped_evtrigs = true;
\echo

create table supa_stuff();

reset supautils.log_skipped_evtrigs;
\echo

-- privesc won't happen because the event trigger function will fire with the privileges
-- of the current role (this is pg default behavior)
set role rolecreator;
\echo

create table dummy();
\echo

-- a non-privileged role can't alter event triggers
set role rolecreator;
alter event trigger event_trigger_1 disable;
\echo

-- the privileged role can alter its own event triggers
set role privileged_role;
alter event trigger event_trigger_1 disable;
\echo

-- a non-privileged role can't drop the event triggers
set role rolecreator;
drop event trigger event_trigger_1;
\echo

-- the privileged role can drop its own event triggers
set role privileged_role;
drop event trigger event_trigger_1;
drop event trigger event_trigger_2;
