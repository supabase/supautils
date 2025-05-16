-- enable logging skipped event triggers
set supautils.log_skipped_evtrigs = true;
\echo

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

-- creating extensions will not fire evtrigs
set role privileged_role;
create extension postgres_fdw;
drop extension postgres_fdw;
\echo

-- creating fdws will not fire evtrigs
create foreign data wrapper new_fdw;
-- TODO: while correct, this is inconsistent as dropping the fdw does fire the evtrig for the privileged_role
drop foreign data wrapper new_fdw;
\echo

-- creating pubs will not fire evtrigs
create publication p for all tables;
-- TODO: while correct, this is inconsistent as dropping the publication does fire the evtrig for the privileged_role
drop publication p;
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
\echo

select count(*) = 1 as only_one_super from pg_roles where rolsuper;

-- ensure logging doesn't happen when the GUC is disabled
set supautils.log_skipped_evtrigs = false;
\echo

create table supa_stuff();
\echo

set role supabase_storage_admin;
\echo

create table some_stuff();
\echo

-- restablish logging for the rest of the tests
set supautils.log_skipped_evtrigs = true;
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
\echo

-- security definer function case
set role privileged_role;
create function secdef_show_current_user()
    returns event_trigger
    language plpgsql
    security definer as
$$
begin
    raise notice 'the event trigger is executed for %', current_user;
end;
$$;
create event trigger event_trigger_3 on ddl_command_end
execute procedure secdef_show_current_user();
\echo

-- secdef won't be executed for superuser
set role postgres;
create table super_foo();
\echo

-- secdef won't be executed for reserved roles
set role supabase_storage_admin;
create table storage_foo();
\echo

-- secdef will be executed for other roles
set role rolecreator;
create table rolecreator_foo();
\echo

-- secdef will be executed for privileged_role
set role privileged_role;
create table privileged_role_foo();
\echo

-- cleanup
set role privileged_role;
drop event trigger event_trigger_3;
