# supautils

<p>

<a href="https://github.com/supabase/supautils/actions"><img src="https://github.com/supabase/supautils/actions/workflows/main.yml/badge.svg" alt="Tests" height="18"></a>
<a href="https://github.com/olirice/alembic_utils/blob/master/LICENSE"><img src="https://img.shields.io/pypi/l/markdown-subtemplate.svg" alt="License" height="18"></a>
<a href=""><img src="https://img.shields.io/badge/postgresql-13+-blue.svg" alt="PostgreSQL version" height="18"></a>

</p>

Supautils is an extension that secures a PostgreSQL cluster on a cloud environment.

It doesn't require creating database objects. It's a shared library that modifies PostgreSQL behavior through "hooks", not through tables or functions.

It's tested to work on PostgreSQL 13, 14, 15, and 16.

## Installation

Clone this repo and run

```bash
make && make install
```

To make it available on some PostgreSQL roles

```
ALTER ROLE role1 SET session_preload_libraries TO 'supautils';
ALTER ROLE role2 SET session_preload_libraries TO 'supautils';
```

Or to make it available to the whole cluster

```
# add the following on postgresql.conf
# use SHOW config_file; for finding the location
shared_preload_libraries = 'supautils'
```

## Role Security

This functionality prevents non-superusers from modifying/granting a set of roles.

Say your backend service depends on a `connector` role for connecting to the database. Also, you need to give database users the ability to create their own roles, i.e. they need the CREATEROLE privilege.

A problem arises here, because any database user with CREATEROLE can DROP or ALTER the `connector` role, making your backend service fail.
From [role attributes docs](https://www.postgresql.org/docs/current/role-attributes.html):

> A role with CREATEROLE privilege can **alter and drop other roles, too, as well as grant or revoke membership in them**.
> However, to create, alter, drop, or change membership of a superuser role, superuser status is required;
> CREATEROLE is insufficient for that.

The above problem can be solved by configuring this extension to protect the `connector` role:

```
supautils.reserved_roles = 'connector'
```

This extension also allows restricting memberships grants for a set of roles. Certain default postgres roles are dangerous
to expose to every database user. From [pg default roles](https://www.postgresql.org/docs/11/default-roles.html):

> The pg_read_server_files, pg_write_server_files and pg_execute_server_program roles are intended to allow administrators to have trusted,
> but non-superuser, roles which are able to access files and run programs on the database server as the user the database runs as.
> As these roles are able to access any file on the server file system, they bypass all database-level permission checks when accessing files directly
> and **they could be used to gain superuser-level access**, therefore great care should be taken when granting these roles to users.

For example, you can restrict doing `GRANT pg_read_server_files TO my_role` with:

```
supautils.reserved_memberships = 'pg_read_server_files'
```

### Configuration

Settings available:

```
supautils.reserved_roles = 'supabase_admin,supabase_auth_admin,supabase_storage_admin'
supautils.reserved_memberships = 'pg_read_server_files, pg_write_server_files, pg_execute_server_program'
```

## Privileged Extensions

This functionality is adapted from [pgextwlist](https://github.com/dimitri/pgextwlist).

supautils allows you to let non-superusers manage extensions that would normally require being a superuser. e.g. the `hstore` extension creates a base type, which requires being a superuser to perform.

To handle this, you can put the extension in `supautils.privileged_extensions`:

```
supautils.privileged_extensions = 'hstore'
supautils.privileged_extensions_superuser = 'postgres'
```

Once you do, the extension creation will be delegated to the configured `supautils.privileged_extensions_superuser` (defaults to the bootstrap user, i.e. the role used to bootstrap the Postgres cluster). That means the `hstore` extension would be created as if by the superuser.

Note that extension creation would behave normally (i.e. no delegation) if the current role is already a superuser.

This also works for updating and dropping privileged extensions.

If you don't want to enable this functionality, simply leave `supautils.privileged_extensions` empty. Extensions **not** in `supautils.privileged_extensions` would behave normally, i.e. created using the current role.

supautils also lets you set custom scripts per privileged extension that gets run at certain events. Currently supported scripts are `before-create` and `after-create`.

To make this work, configure the setting below:

```
supautils.privileged_extensions_custom_scripts_path = '/opt/postgresql/privileged_extensions_custom_scripts'
```

Then put the scripts inside the path, e.g.:

```sql
-- /opt/postgresql/privileged_extensions_custom_scripts/hstore/after-create.sql
grant all on type hstore to non_superuser_role;
```

This is useful for things like creating a dedicated role per extension and granting privileges as needed to that role.

### Configuration

Settings available:

```
supautils.privileged_extensions = 'hstore,moddatetime'
supautils.privileged_extensions_custom_scripts_path = '/opt/postgresql/privileged_extensions_custom_scripts'
supautils.privileged_extensions_superuser = 'postgres'
```

## Constrained Extensions

You can constrain the resources needed for an extension to be installed. This is done through:

```
supautils.constrained_extensions = '{"plrust": {"cpu": 16, "mem": "1 GB", "disk": "500 MB"}, "any_extension_name": { "mem": "1 GB"}}'
```

The `supautils.constrained_extensions` is a json object, any other json type will result in an error.

Each top field of the json object corresponds to an extension name, the only value these top fields can take is a json object composed of 3 keys: `cpu`, `mem` and `disk`.

- `cpu`: is the minimum number of cpus this extension needs. It's a json number.
- `mem`: is the minimum amount of memory this extension needs. It's a json string that takes a human-readable format of bytes.
- `disk`: is the minimum amount of free disk space this extension needs. It's a json string that takes a human-readable format of bytes.
  + The free space of the disk is taken from the filesystem where PGDATA (data directory) is located.

Note: this human-readable format is the same that [pg_size_pretty](https://pgpedia.info/p/pg_size_pretty.html) would give.

`CREATE EXTENSION` will fail if any of the resource constraints are not met:

```sql
create extension plrust;

ERROR:  not enough CPUs for using this extension
DETAIL:  required CPUs: 16
HINT:  upgrade to an instance with higher resources
```

## Extensions Parameter Overrides

You can override `CREATE EXTENSION` parameters like so:

```
supautils.extensions_parameter_overrides = '{ "pg_cron": { "schema": "pg_catalog" } }'
```

Currently, only the `schema` parameter is supported.

These overrides will apply on `CREATE EXTENSION`, e.g.:

```sql
postgres=> create extension pg_cron schema public;
CREATE EXTENSION
postgres=> \dx pg_cron
                 List of installed extensions
  Name   | Version |   Schema   |         Description
---------+---------+------------+------------------------------
 pg_cron | 1.5     | pg_catalog | Job scheduler for PostgreSQL
(1 row)
```

## Managing Policies Without Table Ownership

In Postgres, only table owners can create RLS policies for a table. This can be
limiting if you need to allow certain roles to manage policies without allowing
them to perform other DDL (e.g. to prevent them from dropping the table).

With supautils, this can be done like so:

```
supautils.policy_grants = '{ "my_role": ["public.not_my_table", "public.also_not_my_table"] }'
```

This allows `my_role` to manage policies for `public.not_my_table` and
`public.also_not_my_table` without being an owner of these tables.

## Development

[Nix](https://nixos.org/download.html) is required to set up the environment.

### Testing

For testing the module locally, execute:

```bash
# might take a while in downloading all the dependencies
$ nix-shell

# test on pg 13
$ supautils-with-pg-13 make installcheck

# test on pg 14
$ supautils-with-pg-14 make installcheck

# you can also test manually with
$ supautils-with-pg-13 psql -U rolecreator
```
