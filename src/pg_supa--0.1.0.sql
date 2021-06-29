create function get_one()
returns text as
$$ select 1;
$$ language sql immutable strict;
