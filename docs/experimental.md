# Privileged Extensions

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

## Configuration

Settings available in `postgresql.conf`:

```
supautils.privileged_extensions = 'hstore,moddatetime'
supautils.privileged_extensions_custom_scripts_path = '/opt/postgresql/privileged_extensions_custom_scripts'
supautils.privileged_extensions_superuser = 'postgres'
```
