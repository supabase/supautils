select supa.sql_to_ast($$
    select name, id, dob
    from public.uSer
    where id = 'hello\nworld'
    limit 1
$$::text);


select supa.sql_to_ast($$
    create table acct(
        id int primary key
    )
$$::text);
