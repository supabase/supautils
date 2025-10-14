-- skipped event triggers are logged
set supautils.log_skipped_evtrigs = true;
\echo

-- a superuser can create an event trigger
set role postgres;
\echo

create function super_show_current_user()
    returns event_trigger
    language plpgsql as
$$
begin
    raise notice 'Executing superuser event trigger: current_user is %', current_user;
end;
$$;
\echo

create event trigger event_trigger_1 on ddl_command_end
execute procedure super_show_current_user();
\echo

-- the event trigger is owned by the superuser
select evtowner::regrole from pg_event_trigger where evtname = 'event_trigger_1';

-- the superuser evtrig will execute for superuser
create table super_thing();
\echo

-- another superuser can create an event trigger
create role super2 superuser;
set role super2;
create function super2_show_current_user()
    returns event_trigger
    language plpgsql as
$$
begin
    raise notice 'Executing super2 event trigger: current_user is %', current_user;
end;
$$;
create event trigger super2_event_trigger on ddl_command_end
execute procedure super2_show_current_user();
\echo

-- only one superuser evtrig will execute for superuser (the one he owns)
create table super2_thing();
\echo

-- super2 evtrigs won't run here, only the postgres owned evtrigs
set role postgres;
create table super_duper_thing();
\echo

-- super2 cleanup
drop event trigger super2_event_trigger;
drop function super2_show_current_user;
drop table super2_thing;
drop role super2;
\echo

-- A regular user is not able to create event trigger using a superuser function
set role privileged_role;
\echo

create event trigger event_trigger_2 on ddl_command_end
execute procedure super_show_current_user();
\echo

-- A reserved role will execute the superuser evtrig
set role supabase_storage_admin;
\echo

create table storage_stuff();
\echo

drop table storage_stuff;
\echo

-- creating extensions will fire superuser evtrigs
set role privileged_role;
create extension postgres_fdw;
drop extension postgres_fdw;
\echo

-- creating fdws will fire superuser evtrigs
create foreign data wrapper new_fdw;
drop foreign data wrapper new_fdw;
\echo

-- creating publications will fire superuser evtrigs
create publication p for all tables;
drop publication p;
\echo

-- a non-privileged role cannot alter a superuser owned evtrig
set role rolecreator;
\echo

alter event trigger event_trigger_1 disable;
\echo

-- the privileged role cannot alter a superuser owned evtrig
set role privileged_role;
\echo

alter event trigger event_trigger_1 disable;
\echo

-- a reserved role cannot alter a superuser owned evtrig
set role supabase_storage_admin;
\echo

alter event trigger event_trigger_1 disable;
\echo

-- only the superuser can alter its own event triggers
set role postgres;
alter event trigger event_trigger_1 disable;
\echo

-- a non-privileged role cannot drop a superuser owned evtrig
set role rolecreator;
\echo

drop event trigger event_trigger_1;
\echo

-- the privileged role cannot drop a superuser owned evtrig
set role privileged_role;
\echo

drop event trigger event_trigger_1;
\echo

-- a reserved role cannot drop a superuser owned evtrig
set role supabase_storage_admin;
\echo

drop event trigger event_trigger_1;
\echo

-- only the superuser can drop its own event triggers
set role postgres;
drop event trigger event_trigger_1;
\echo

-- disable logging event triggers
reset supautils.log_skipped_evtrigs;
