Tested to work on PostgreSQL 12, 13 and 14.

## Setup

### Server
Clone this repo and run

```bash
make && make install
```

To make the extension available to the database add on `postgresql.conf`:

```
shared_preload_libraries = 'supautils'
```


### Database
To enable the extension in PostgreSQL we must execute a `create extension` statement. It is reccomended to create `supautils` in a schema to avoid naming conflicts.

```psql
create schema supa;
create extension supautils with schema supa;
```
