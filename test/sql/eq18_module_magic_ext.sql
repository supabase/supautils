select
  module_name,
  version ~ '^[0-9]+\.[0-9]+\.[0-9]+$' as version_is_semver
from pg_get_loaded_modules()
where module_name = 'supautils';
