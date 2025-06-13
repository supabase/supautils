-- create the extension as usual
create extension if not exists pgmq;
set supautils.log_skipped_evtrigs = true;
\echo

-- downgrade the extension functions owners to non-superuser, to ensure the following function calls are not wrongly skipped by the event trigger mechanism
do $$
declare
  extoid oid := (select oid from pg_extension where extname = 'pgmq');
  r record;
  cls pg_class%rowtype;
begin
for r in (select * from pg_depend where refobjid = extoid) loop
    if r.classid = 'pg_proc'::regclass then
      execute(format('alter function %s(%s) owner to privileged_role;', r.objid::regproc, pg_get_function_identity_arguments(r.objid)));
    end if;
end loop;
end $$;
\echo

-- Test the standard flow
select
  pgmq.create('Foo');

select
  *
from
  pgmq.send(
    queue_name:='Foo',
    msg:='{"foo": "bar1"}'
  );

-- Test queue is not case sensitive
select
  *
from
  pgmq.send(
    queue_name:='foo', -- note: lowercase useage
    msg:='{"foo": "bar2"}',
    delay:=5
  );

select
  msg_id,
  read_ct,
  message
from
  pgmq.read(
    queue_name:='Foo',
    vt:=30,
    qty:=2
  );

select
  msg_id,
  read_ct,
  message
from
  pgmq.pop('Foo');


-- Archive message with msg_id=2.
select
  pgmq.archive(
    queue_name:='Foo',
    msg_id:=2
  );


select
  pgmq.create('my_queue');

select
  pgmq.send_batch(
  queue_name:='my_queue',
  msgs:=array['{"foo": "bar3"}','{"foo": "bar4"}','{"foo": "bar5"}']::jsonb[]
);

select
  pgmq.archive(
    queue_name:='my_queue',
    msg_ids:=array[3, 4, 5]
  );

select
  pgmq.delete('my_queue', 6);


select
  pgmq.drop_queue('my_queue');
