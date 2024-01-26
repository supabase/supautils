GREP ?= grep
PG_CONFIG = pg_config

ifeq ($(TEST), 1)
	PG_CFLAGS = -Wall -Werror -DTEST
else
	PG_CFLAGS = -Wall -Werror
endif

MODULE_big = supautils
OBJS = src/supautils.o src/privileged_extensions.o src/constrained_extensions.o src/utils.o

PG_VERSION = $(strip $(shell $(PG_CONFIG) --version | $(GREP) -oP '(?<=PostgreSQL )[0-9]+'))
SYSTEM = $(shell uname -s)

ifneq ($(SYSTEM), Linux)
TESTS = $(filter-out test/sql/ge13_%.sql, $(wildcard test/sql/*.sql))
else
TESTS = $(wildcard test/sql/*.sql)
endif

REGRESS = $(patsubst test/sql/%.sql,%,$(TESTS))
REGRESS_OPTS = --use-existing --inputdir=test

PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)
