GREP ?= grep
PG_CONFIG = pg_config

PG_CFLAGS = -std=c99 -Wextra -Wall -Werror -Wno-declaration-after-statement
ifeq ($(TEST), 1)
	PG_CFLAGS += -DTEST
endif

MODULE_big = supautils
OBJS = src/supautils.o src/privileged_extensions.o src/drop_trigger_grants.o src/constrained_extensions.o src/extensions_parameter_overrides.o src/policy_grants.o src/utils.o

PG_VERSION = $(strip $(shell $(PG_CONFIG) --version | $(GREP) -oP '(?<=PostgreSQL )[0-9]+'))
PG_GE16 = $(shell test $(PG_VERSION) -ge 16; echo $$?)
SYSTEM = $(shell uname -s)

TESTS := $(wildcard test/sql/*.sql)
ifneq ($(SYSTEM), Linux)
TESTS := $(filter-out test/sql/linux_%.sql, $(TESTS))
endif
ifneq ($(PG_GE16), 0)
TESTS := $(filter-out test/sql/ge16_%.sql, $(TESTS))
else
TESTS := $(filter-out test/sql/lt16_%.sql, $(TESTS))
endif

REGRESS = $(patsubst test/sql/%.sql,%,$(TESTS))
REGRESS_OPTS = --use-existing --inputdir=test

PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)
