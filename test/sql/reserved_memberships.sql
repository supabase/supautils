-- use a role that has the CREATEROLE privilege
set role rolecreator;
\echo

-- cannot grant reserved memberships
grant pg_read_server_files to anon;
grant pg_write_server_files to anon;
grant pg_execute_server_program to anon;
\echo

-- can grant non-reserved memberships
grant pg_monitor to anon;
grant pg_read_all_settings to anon;
grant pg_read_all_stats to anon;
