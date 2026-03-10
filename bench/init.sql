create role privileged_role nosuperuser login createrole bypassrls replication;
grant all on database postgres to privileged_role;
grant all on schema public to privileged_role;
grant all on all tables in schema public TO privileged_role;

create role hint_role nosuperuser login createrole bypassrls replication;
grant all on database postgres to hint_role;
grant all on schema public to hint_role;
grant all on all tables in schema public TO hint_role;
