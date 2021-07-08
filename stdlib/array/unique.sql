create or replace function @extschema@.unique(arr anyarray) returns anyarray
as $$
	select
		array_agg(distinct val)
	from
		unnest(arr) xyz(val)
	limit
		1
$$ language sql immutable strict parallel safe;


