## Supautils

Miscellaneous PostgreSQL library for Supabase.

### Installation

```bash
> make && make install
```

On `postgresql.conf`, add:

```
shared_preload_libraries = 'check_role_membership'
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

> grant pg_execute_server_program to postgres;

2021-04-25 15:31:28.541 -05 [23879] ERROR:  Cannot GRANT "pg_execute_server_program"
2021-04-25 15:31:28.541 -05 [23879] STATEMENT:  grant pg_execute_server_program to postgres ;
ERROR:  42501: Cannot GRANT "pg_execute_server_program"
LOCATION:  check_role_membership, check_role_membership.c:62
```

For pg 13:

```bash
> supautils-pg-13

psql (13.1)
Type "help" for help.

> grant pg_read_server_files to postgres;

2021-04-25 15:31:55.247 -05 [23908] ERROR:  Cannot GRANT "pg_read_server_files"
2021-04-25 15:31:55.247 -05 [23908] STATEMENT:  grant pg_read_server_files to postgres;
ERROR:  42501: Cannot GRANT "pg_read_server_files"
LOCATION:  check_role_membership, check_role_membership.c:57
```
