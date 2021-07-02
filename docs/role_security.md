The supautils extension provides tooling to prevent non-superusers from modifying/granting a set of roles.

Say your backend service depends on a `connector` role for connecting to the database. Also, you need to give database users the ability to create their own roles, i.e. they need the CREATEROLE privilege.

A problem arises here, because any database user with CREATEROLE can DROP or ALTER the `connector` role, making your backend service fail.
From [role attributes docs](https://www.postgresql.org/docs/current/role-attributes.html):

> A role with CREATEROLE privilege can **alter and drop other roles, too, as well as grant or revoke membership in them**.
> However, to create, alter, drop, or change membership of a superuser role, superuser status is required;
> CREATEROLE is insufficient for that.

The above problem can be solved by configuring this extension to protect the `connector` role:

```
supautils.reserved_roles = "connector"
```

This extension also allows restricting memberships grants for a set of roles. Certain default postgres roles are dangerous
to expose to every database user. From [pg default roles](https://www.postgresql.org/docs/11/default-roles.html):

> The pg_read_server_files, pg_write_server_files and pg_execute_server_program roles are intended to allow administrators to have trusted,
> but non-superuser, roles which are able to access files and run programs on the database server as the user the database runs as.
> As these roles are able to access any file on the server file system, they bypass all database-level permission checks when accessing files directly
> and **they could be used to gain superuser-level access**, therefore great care should be taken when granting these roles to users.

For example, you can restrict doing `GRANT pg_read_server_files TO my_role` with:

```
supautils.reserved_memberships = "pg_read_server_files"
```

## Configuration

Settings available in `postgresql.conf`:

### Protect Roles
```
supautils.reserved_roles = "supabase_admin,supabase_auth_admin,supabase_storage_admin"
```

### Protect Memberships
```
supautils.reserved_memberships = "pg_read_server_files, pg_write_server_files, pg_execute_server_program"
```


