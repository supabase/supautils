-- use a role that has the CREATEROLE privilege
set role rolecreator;
\echo

-- cannot grant reserved memberships
grant pg_read_server_files to anon;
grant pg_write_server_files to anon;
grant pg_execute_server_program to anon;
\echo

-- cannot grant reserved memberships when creating a role
create role tester in role pg_read_server_files;
create role tester in group anon, pg_write_server_files;
create role tester in role pg_execute_server_program, anon;
create role role_with_reserved_membership admin anon;
\echo

-- can grant non-reserved memberships
grant pg_monitor to anon;
grant pg_read_all_settings to anon;
grant pg_read_all_stats to anon;
\echo

-- can grant non-reserved memberships when creating a role
create role tester in role anon;
create role other admin anon;
