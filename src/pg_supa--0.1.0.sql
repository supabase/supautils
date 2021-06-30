create function @extschema@.get_one()
returns int as
$$ select 1;
$$ language sql immutable strict;
