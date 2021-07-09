create or replace function @extschema@.table_row_count_estimate(entity regclass) returns bigint
as $$
    select
        reltuples::bigint
    from
        pg_class
    where
        oid = entity::oid;
$$ language sql strict;
