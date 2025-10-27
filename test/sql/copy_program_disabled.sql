-- create a superuser role
create role super2 superuser;
set role super2;
\echo

-- superuser can't execute COPY ... PROGRAM
copy (select '') to program 'id';
\echo

-- superuser can't disable guc
set supautils.disable_program = false;
\echo

-- current_setting must be 'on'
select current_setting('supautils.disable_program');
alter system set supautils.disable_program = false;
select  pg_reload_conf();
--  altering the system config does  not change the state of supautils.disable_program
select current_setting('supautils.disable_program');
\echo

-- cleanup
set role postgres;
drop role super2;
\echo
