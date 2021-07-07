select supa.sql_to_ast($$
    create table acct(
        id int primary key
    )
$$::text);
