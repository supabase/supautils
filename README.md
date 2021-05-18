## Supautils

This extension prevents modifying a set of roles by non-superusers.

Say your backend service depends on a `connector` role(not a SUPERUSER) for connecting to the database. Also, you need to give database users the ability to create their own roles, i.e. they need the CREATEROLE privilege.

A problem arises here, because any database user with CREATEROLE can DROP or ALTER the `connector` role, making your backend service fail.
From [role attributes docs](https://www.postgresql.org/docs/current/role-attributes.html):

> A role with CREATEROLE privilege can alter and drop other roles, too, as well as grant or revoke membership in them.
> However, to create, alter, drop, or change membership of a superuser role, superuser status is required;
> CREATEROLE is insufficient for that.

The above problem can be solved by configuring this extension to protect the `connector`role from non-superusers:

```
supautils.reserved_roles = "connector"
```

This extension also allows restricting memberships grants for a set of roles. Certain default postgres roles are dangerous
to expose to every database user. From [pg default roles](https://www.postgresql.org/docs/11/default-roles.html):

> The pg_read_server_files, pg_write_server_files and pg_execute_server_program roles are intended to allow administrators to have trusted,
> but non-superuser, roles which are able to access files and run programs on the database server as the user the database runs as.
> As these roles are able to access any file on the server file system, they bypass all database-level permission checks when accessing files directly
> and **they could be used to gain superuser-level access**, therefore great care should be taken when granting these roles to users.

You can restrict doing `GRANT pg_read_server_files TO my_role` to non-superusers with:

```
supautils.reserved_memberships = "pg_read_server_files"
```

In general, this extension aims to make the CREATEROLE privilege safer.

### Installation

Tested to work on PostgreSQL 12 and 13.

Clone this repo and do:

```bash
make && make install
```

### Usage

To enable the extension, add on `postgresql.conf`:

```
shared_preload_libraries = 'supautils'
```

Protect any list of roles:

```
supautils.reserved_roles = "supabase_admin,supabase_auth_admin,supabase_storage_admin"
```

Protect any list of memberships:

```
supautils.reserved_memberships = "pg_read_server_files, pg_write_server_files, pg_execute_server_program"
```

### Development

[Nix](https://nixos.org/download.html) is used to set up the environment.

For testing the module you can do:

```bash
# might take a while in downloading all the dependencies
$ nix-shell

# test on pg 12
$ supautils-with-pg-12 make installcheck

# test on pg 13
$ supautils-with-pg-13 make installcheck

# you can also test manually with
$ supautils-with-pg-12 psql -U rolecreator
```
