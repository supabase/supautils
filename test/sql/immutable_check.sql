create table products (
    product_no integer,
    date timestamptz check (now() > '2025-03-01')
);
\echo

create table products (
    product_no integer,
    name text check (name <> current_setting('request.name', true))
);
