PG_CFLAGS = -Wall -Werror

DATA = $(wildcard sql/*--*.sql)

MODULE_big = supautils
OBJS = src/supautils.o src/privileged_extensions.o src/utils.o

TESTS = $(wildcard test/sql/*.sql)
REGRESS = $(patsubst test/sql/%.sql,%,$(TESTS))
REGRESS_OPTS = --use-existing --inputdir=test

# Tell pg_config to pass us the PostgreSQL extensions makefile(PGXS)
# and include it into our own Makefile through the standard "include" directive.
PG_CONFIG = pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)
