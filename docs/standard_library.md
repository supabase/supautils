## Library Reference

The [supabase](https://supabase.io/) platform nudges users to collocate  compute with data than is typical for PostgreSQL backed applications. As a result, the PostgreSQL standard library

All examples assume `supautils` was created in a schema named `supa`.

### Array

#### index

Return one-based index in the array of the first item whose value is equal to x. Returns `null` if there is no such item.

```sql
--8<-- "test/expected/index.out"
```

#### reverse

Return an array that is in reverse order from the input array.

```sql
--8<-- "test/expected/reverse.out"
```

#### unique

Return an array containing unique elements of the input array.

```sql
--8<-- "test/expected/unique.out"
```
