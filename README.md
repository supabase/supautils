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
```
