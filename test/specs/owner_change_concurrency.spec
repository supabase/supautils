# The privileged role creates an event trigger or a foreign data wrapper while
# another session does the same, or alters the role. Nobody waits and nobody
# fails.
#
# The owner change used to make the role a superuser and back, two updates of
# its pg_authid row. The other session waited for the first transaction to end
# and then failed with "tuple concurrently updated".

setup
{
  create extension postgres_fdw;
  create function iso_noop() returns event_trigger language plpgsql as $$ begin end $$;
  alter function iso_noop() owner to privileged_role;
}

teardown
{
  drop function iso_noop() cascade;
  drop extension postgres_fdw cascade;
}

session s1
setup           { set role privileged_role; }
step s1_begin   { begin; }
step s1_evtrig  { create event trigger iso_evtrig_1 on ddl_command_end execute procedure iso_noop(); }
step s1_commit  { commit; }

session s2
setup           { set role privileged_role; }
step s2_evtrig  { create event trigger iso_evtrig_2 on ddl_command_end execute procedure iso_noop(); }
step s2_fdw     { create foreign data wrapper iso_fdw handler postgres_fdw_handler validator postgres_fdw_validator; }
step s2_owners
{
  select evtname as name, evtowner::regrole as owner from pg_event_trigger where evtname like 'iso_evtrig_%'
  union all
  select fdwname, fdwowner::regrole from pg_foreign_data_wrapper where fdwname = 'iso_fdw'
  order by 1;
}

# a superuser, as when the role's password is rotated
session s3
step s3_alter_role { alter role privileged_role connection limit -1; }

permutation s1_begin s1_evtrig s2_evtrig s1_commit s2_owners
permutation s1_begin s1_evtrig s2_fdw s1_commit s2_owners
permutation s1_begin s1_evtrig s3_alter_role s1_commit
