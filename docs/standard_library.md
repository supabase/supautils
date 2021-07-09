## Library Reference

The [supabase](https://supabase.io/) platform nudges users to collocate  compute with data than is typical for PostgreSQL backed applications. As a result, the PostgreSQL standard library

All examples assume `supautils` was created in a schema named `supa`.

### Array

#### index

Return one-based index in the array of the first item whose value is equal to x. Returns `null` if there is no such item.

```sql
--8<-- "test/expected/stdlib/array/index.out"
```

#### reverse

Return an array that is in reverse order from the input array.

```sql
--8<-- "test/expected/stdlib/array/reverse.out"
```

#### unique

Return an array containing unique elements of the input array.

```sql
--8<-- "test/expected/stdlib/array/unique.out"
```


### Inspect

#### table_row_count_estimate
Return the approximate number of rows in a table.

```sql
--8<-- "test/expected/stdlib/inspect/table_row_count_estimate.out"
```


#### query_row_count_estimate
Return the approximate number of rows to be selected in a query.

```sql
--8<-- "test/expected/stdlib/inspect/query_row_count_estimate.out"
```

