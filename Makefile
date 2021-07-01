EXTENSION = supautils
DATA = $(wildcard sql/*--*.sql)

MODULE_big = supautils
OBJS = src/supautils.o
TESTS = $(wildcard test/sql/*.sql)
REGRESS = $(patsubst test/sql/%.sql,%,$(TESTS))
REGRESS_OPTS = --use-existing --inputdir=test

PG_CONFIG = pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)
