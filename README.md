## Supautils

Miscellaneous PostgreSQL library for Supabase.

### Installation

```bash
> make && make install
```

On `postgresql.conf`, add:

```
shared_preload_libraries = 'supautils'

supautils.reserved_roles = "authenticator,anon,authenticated,service_role,supabase_admin,supabase_auth_admin,supabase_storage_admin"

supautils.reserved_memberships = "pg_read_server_files, pg_write_server_files, pg_execute_server_program"
```
