EXTENSION = supautils
DATA = $(wildcard sql/*--*.sql)

MODULE_big = supautils
OBJS = src/supautils.o src/allowed_extensions.o

TESTS = $(wildcard test/sql/*.sql test/sql/stdlib/array/*.sql test/sql/stdlib/inspect/*.sql)
REGRESS = $(patsubst test/sql/%.sql,%,$(TESTS))
REGRESS_OPTS = --use-existing --inputdir=test

# Tell pg_config to pass us the PostgreSQL extensions makefile(PGXS)
# and include it into our own Makefile through the standard "include" directive.
PG_CONFIG = pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)
