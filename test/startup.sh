# To test the timezone set in the startup packet we use the following
# because new failing connections can't be part of pg_regress based tests

set -euo pipefail

# The BUILD_DIR variable contains values like build-<version> so we
# strip the build- portion to recover the version.
pg_version="${BUILD_DIR#build-}"

echo "TEST: invalid timezone in startup packet fails"

if output=$(
  xpg -v "$pg_version" psql "options='-c timezone=XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX0'" \
    -c "select 1" 2>&1
); then
  echo "Expected psql to fail" >&2
  exit 1
fi

if [[ $output != *'FATAL:  time zone abbreviation '* ]] ; then
  echo "Unexpected error output" >&2
  printf '%s\n' "$output" >&2
  exit 1
fi

echo "TEST: valid timezones should pass successfully"

if output=$(
  xpg -v "$pg_version" psql "options='-c timezone=posix/America/Argentina/ComodRivadavia'" \
    -c "select 1" 2>&1 >/dev/null
); then
  :
else
  status=$?
  printf '%s\n' "$output" >&2
  exit "$status"
fi
