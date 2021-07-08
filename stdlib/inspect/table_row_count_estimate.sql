create or replace function @extschema@.table_row_count_estimate(entity regclass) returns int
as $$
    select
        reltuples
    from
        pg_class
    where
        oid = entity::oid;
$$ language sql strict;
