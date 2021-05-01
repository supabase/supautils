## Supautils

Miscellaneous PostgreSQL library for Supabase.

### Installation

```bash
> make && make install
```

On `postgresql.conf`, add:

```
shared_preload_libraries = 'supautils'
```

### Development

[Nix](https://nixos.org/download.html) is required.

For testing the module do:

```bash
> nix-shell
```

(Might take a while in downloading all the dependencies)

And then, for pg 12:

```bash
> supautils-pg-12

psql (12.5)
Type "help" for help.

> grant pg_read_server_files to nosuper;

ERROR:  Only superusers can GRANT "pg_read_server_files"
STATEMENT:  grant pg_read_server_files to nosuper;

> grant pg_write_server_files to nosuper;

ERROR:  Only superusers can GRANT "pg_write_server_files"
STATEMENT:  grant pg_write_server_files to nosuper;

> grant pg_execute_server_program to nosuper;

ERROR:  Only superusers can GRANT "pg_execute_server_program"
STATEMENT:  grant pg_execute_server_program to nosuper;
```

For testing on pg 13 use `supautils-pg-13`.
