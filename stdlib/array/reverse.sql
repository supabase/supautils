create or replace function @extschema@.reverse(arr anyarray) returns anyarray
as $$
	select
		array_agg(val order by ix desc)
	from
		unnest(arr) with ordinality xyz(val, ix)
	limit
		1
$$ language sql immutable strict parallel safe;


