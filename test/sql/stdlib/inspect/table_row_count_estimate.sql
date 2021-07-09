begin;
    
    select *
    into public.account
    from generate_series(1, 100);

    analyze public.account;

    select supa.table_row_count_estimate('public.account'::regclass);

rollback;
