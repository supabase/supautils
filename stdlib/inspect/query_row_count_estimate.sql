create or replace function @extschema@.query_row_count_estimate(query text) returns bigint
as $$
    declare
        explain_row record;
        n_rows bigint;
    begin
        for explain_row in execute 'explain ' || query loop
            n_rows := substring(explain_row."QUERY PLAN" FROM ' rows=([[:digit:]]+)');
            exit when n_rows is not null;
        end loop;

        return n_rows;

    end;
$$ language plpgsql strict;


