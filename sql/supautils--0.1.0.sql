create or replace function @extschema@.unique(arr anyarray) returns anyarray
as $$
	select
		array_agg(distinct val)
	from
		unnest(arr) xyz(val)
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


create or replace function @extschema@.query_row_count_estimate(query text) returns int
as $$
    declare
        explain_row record;
        n_rows integer;
    begin
        for explain_row in execute 'explain ' || query loop
            n_rows := substring(explain_row."QUERY PLAN" FROM ' rows=([[:digit:]]+)');
            exit when n_rows is not null;
        end loop;

        return n_rows;

    end;
$$ language plpgsql strict;


create or replace function @extschema@.table_row_count_estimate(entity regclass) returns int
as $$
    select
        reltuples
    from
        pg_class
    where
        oid = entity::oid;
$$ language sql strict;
