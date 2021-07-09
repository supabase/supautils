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


