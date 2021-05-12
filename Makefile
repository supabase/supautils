MODULE_big = supautils
SOURCES = src/supautils.c
OBJS = src/supautils.o
REGRESS = reserved_roles non_reserved_roles
REGRESS_OPTS = --inputdir=test

PG_CONFIG = pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)
