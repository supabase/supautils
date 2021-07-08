select supa.query_row_count_estimate($$
    select *
    from generate_series(1, 100);
$$::text);
