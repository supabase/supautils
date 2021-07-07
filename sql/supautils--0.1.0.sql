-----------
-- ARRAY --
-----------
create or replace function @extschema@.index(arr anyarray, elem anyelement) returns bigint
as $$
	select
		ix
	from
		unnest(arr) with ordinality xyz(val, ix)
	where
		xyz.val = elem
	limit
		1
$$ language sql immutable strict parallel safe;


create or replace function @extschema@.reverse(arr anyarray) returns anyarray
as $$
	select
		array_agg(val order by ix desc)
	from
		unnest(arr) with ordinality xyz(val, ix)
	limit
		1
$$ language sql immutable strict parallel safe;


create or replace function @extschema@.unique(arr anyarray) returns anyarray
as $$
	select
		array_agg(distinct val)
	from
		unnest(arr) xyz(val)
	limit
		1
$$ language sql immutable strict parallel safe;


-------------
-- INSPECT --
-------------
create or replace function @extschema@.sql_to_ast(text) returns text
as 'supautils'
language C strict;


