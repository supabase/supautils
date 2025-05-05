# supautils

[![Coverage Status](https://coveralls.io/repos/github/supabase/supautils/badge.svg?branch=master)](https://coveralls.io/github/supabase/supautils?branch=master)

Supautils is an extension that unlocks advanced Postgres features without granting SUPERUSER access.

It's a loadable library that allows creating event triggers, publications, and other highly privileged database objects on cloud deployments where giving SUPERUSER rights to end users isn’t an option.

It's managed entirely by configuration — no tables, functions, or security labels are added to your database. That makes upgrades effortless and lets you apply settings cluster-wide solely via `postgresql.conf`.

Tested to work on PostgreSQL 13, 14, 15, 16 and 17.

## Installation

Clone this repo and run

```bash
make && make install
```

To make supautils available to the whole cluster, you can add the following to `postgresql.conf` (use `SHOW config_file` for finding the location).

```
shared_preload_libraries = 'supautils'
```

Or to make it available only on some PostgreSQL roles use `session_preload_libraries`.

```
ALTER ROLE role1 SET session_preload_libraries TO 'supautils';
```

## Features

- [Privileged Role](#privileged-role)
- [Privileged extensions](#privileged-extensions)
- [Constrained extensions](#constrained-extensions)
- [Extensions Parameter Overrides](#extensions-parameter-overrides)
- [Table Ownership Bypass](#table-ownership-bypass)
- [Reserved Roles](#reserved-roles)
- [Reserved Memberships](#reserved-memberships)

### Privileged Role

PostgreSQL doesn't allow non-superusers to create certain database objects like publications, foreign data wrappers or event triggers. supautils allows creating these by configuring a `supautils.privileged_role`.
This role is a proxy role for a SUPERUSER, which is configured by `supautils.superuser` (defaults to the bootstrap user, i.e. the role used to bootstrap the Postgres cluster).

#### Non-Superuser Publications

The privileged role can create publications. When it executes `create publication`, supautils will detect the statement and:

- It will switch to the `supautils.superuser`, allowing the operation and creating the publication.
- It will change the ownership of the publication to the privileged role.
- Finally, it will switch back to the privileged role.

#### Non-Superuser Foreign Data Wrappers

The privileged role can also execute `create foreign data wrapper..`, the logic followed is analogous to publication creation.

#### Non-Superuser Event Triggers

The privileged role is also able to create event triggers, while adding protection for privilege escalation.

To protect against privilege escalation, the event triggers created by the privileged role:

- Will be executed for non-superusers.
- Will be skipped for superusers.
- Will also be skipped for [Reserved Roles](#reserved-roles).

The skipping behavior can be logged by setting the `supautils.log_skipped_evtrigs` config to true, this is false by default.

Superuser event triggers work as usual, with the additional restriction that the event trigger function must be owned by a superuser.

```sql
create event trigger evtrig on ddl_command_end
execute procedure func(); -- func must be owned by the superuser
```

The privileged role won't be able to ALTER or DROP a superuser event trigger.

> [!IMPORTANT]
> Limitation: privileged role event triggers won't fire when creating publications, foreign data wrappers or extensions.
> This is due to implementation details, since supautils has to switch to `supautils.superuser` when creating the above database objects, and we have to skip privileged role event triggers here to avoid privilege escalation.

#### Non-Superuser Settings

Certain settings like `session_replication_role` can only be set by superusers. The privileged role can be allowed to change these settings by listing them in:

```
supautils.privileged_role_allowed_configs="session_replication_role"
```

Some extensions also have their own superuser settings with a prefix, those can be configured by:

```
supautils.privileged_role_allowed_configs="ext.setting, other.nested"
```

You can also choose to allow all the extension settings by using a wildcard:

```
supautils.privileged_role_allowed_configs="ext.*"
```

### Privileged Extensions

> [!NOTE]
> This functionality is adapted from [pgextwlist](https://github.com/dimitri/pgextwlist).

supautils allows you to let non-superusers manage extensions that would normally require being a superuser. e.g. the `hstore` extension creates a base type, which requires being a superuser to perform.

To handle this, you can put the extension in `supautils.privileged_extensions`:

```psql
supautils.privileged_extensions = 'hstore'
```

Once you do, the extension creation will be delegated to the configured `supautils.superuser`. That means the `hstore` extension would be created as if by the superuser.

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

### Constrained Extensions

You can constrain the resources needed for an extension to be installed. This is done through:

```
supautils.constrained_extensions = '{"plrust": {"cpu": 16, "mem": "1 GB", "disk": "500 MB"}, "any_extension_name": { "mem": "1 GB"}}'
```

The `supautils.constrained_extensions` is a json object, any other json type will result in an error.

Each top field of the json object corresponds to an extension name, the only value these top fields can take is a json object composed of 3 keys: `cpu`, `mem` and `disk`.

- `cpu`: is the minimum number of cpus this extension needs. It's a json number.
- `mem`: is the minimum amount of memory this extension needs. It's a json string that takes a human-readable format of bytes.
  + The human-readable format is the same that [pg_size_pretty](https://pgpedia.info/p/pg_size_pretty.html) returns.
- `disk`: is the minimum amount of free disk space this extension needs. It's a json string that takes a human-readable format of bytes.
  + The free space of the disk is taken from the filesystem where PGDATA (data directory) is located.

`CREATE EXTENSION` will fail if any of the resource constraints are not met:

```sql
create extension plrust;

ERROR:  not enough CPUs for using this extension
DETAIL:  required CPUs: 16
HINT:  upgrade to an instance with higher resources
```

### Extensions Parameter Overrides

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

### Table Ownership Bypass

#### Manage Policies

In Postgres, only table owners can create RLS policies for a table. This can be limiting if you need to allow certain roles to manage policies without allowing them
to perform other DDL (e.g. to prevent them from dropping the table).

With supautils, this can be done like so:

```
supautils.policy_grants = '{ "my_role": ["public.not_my_table", "public.also_not_my_table"] }'
```

This allows `my_role` to manage policies for `public.not_my_table` and `public.also_not_my_table` without being an owner of these tables.

#### Drop Triggers

You can also allow certain roles to drop triggers on a table without being the table owner:

```
supautils.drop_trigger_grants = '{ "my_role": ["public.not_my_table", "public.also_not_my_table"] }'
```

### Reserved Roles

> [!IMPORTANT]
> The CREATEROLE problem is solved starting from PostgreSQL 16.

Roles with the CREATEROLE privilege can ALTER, DROP or GRANT other roles without restrictions.

From [role attributes docs](https://www.postgresql.org/docs/15/role-attributes.html):

> A role with CREATEROLE privilege can **alter and drop other roles, too, as well as grant or revoke membership in them**.
> However, to create, alter, drop, or change membership of a superuser role, superuser status is required;
> CREATEROLE is insufficient for that.

The above problem can be solved by configuring this extension to protect a set of roles, using the `reserved_roles` setting.

```
supautils.reserved_roles = 'connector, storage_admin'
```

Roles with the CREATEROLE privilege cannot ALTER or DROP the above reserved roles.

#### Reserved Roles Settings

By default, reserved roles cannot have their settings changed. However their settings can be modified by the [Privileged Role](#privileged-role) if they're configured like so:

```
supautils.reserved_roles = 'connector*, storage_admin*'
```

### Reserved Memberships

Certain default postgres roles are dangerous to expose to every database user. From [pg default roles](https://www.postgresql.org/docs/11/default-roles.html):

> The pg_read_server_files, pg_write_server_files and pg_execute_server_program roles are intended to allow administrators to have trusted,
> but non-superuser, roles which are able to access files and run programs on the database server as the user the database runs as.
> As these roles are able to access any file on the server file system, they bypass all database-level permission checks when accessing files directly
> and **they could be used to gain superuser-level access**, therefore great care should be taken when granting these roles to users.

Supautils allows you to restrict doing `GRANT pg_read_server_files TO my_role` by setting:

```
supautils.reserved_memberships = 'pg_read_server_files'
```

This is also useful to limit memberships to the [Reserved Roles](#reserved-roles).

## Development

[Nix](https://nixos.org/download.html) is required to set up the environment.

### Testing

For testing the module locally, execute:

```bash
# might take a while in downloading all the dependencies
$ nix-shell

# test on pg 13
$ xpg -v 13 make installcheck

# test on pg 14
$ xpg -v 14 make installcheck

# you can also test manually with
$ xpg -v 13 psql -U rolecreator
```

### Coverage

For coverage, execute:

```bash
$ xpg -v 17 coverage
```
