# supautils

<p>

<a href="https://github.com/supabase/supautils/actions"><img src="https://github.com/supabase/supautils/actions/workflows/main.yml/badge.svg" alt="Tests" height="18"></a>
<a href="https://github.com/olirice/alembic_utils/blob/master/LICENSE"><img src="https://img.shields.io/pypi/l/markdown-subtemplate.svg" alt="License" height="18"></a>
<a href=""><img src="https://img.shields.io/badge/postgresql-12+-blue.svg" alt="PostgreSQL version" height="18"></a>

</p>

## Installation

Tested to work on PostgreSQL 12, 13, 14 and 15.

## Setup

Clone this repo and run

```bash
make && make install
```

To make the extension available to the database add on `postgresql.conf`:

```
shared_preload_libraries = 'supautils'
```

## Role Security

The supautils extension provides tooling to prevent non-superusers from modifying/granting a set of roles.

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

Settings available in `postgresql.conf`:

#### Protect Roles
```
supautils.reserved_roles = 'supabase_admin,supabase_auth_admin,supabase_storage_admin'
```

#### Protect Memberships
```
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

Settings available in `postgresql.conf`:

```
supautils.privileged_extensions = 'hstore,moddatetime'
supautils.privileged_extensions_custom_scripts_path = '/opt/postgresql/privileged_extensions_custom_scripts'
supautils.privileged_extensions_superuser = 'postgres'
```

## Development

[Nix](https://nixos.org/download.html) is required to set up the environment.


### Testing
For testing the module locally, execute:

```bash
# might take a while in downloading all the dependencies
$ nix-shell

# test on pg 12
$ supautils-with-pg-12 make installcheck

# test on pg 13
$ supautils-with-pg-13 make installcheck

# test on pg 14
$ supautils-with-pg-14 make installcheck

# you can also test manually with
$ supautils-with-pg-12 psql -U rolecreator
```
