EXTENSION = pg_supa
DATA = src/pg_supa--0.1.0.sql

MODULE_big = supautils
OBJS = src/supautils.o
REGRESS = reserved_roles reserved_memberships pg_supa
REGRESS_OPTS = --use-existing --inputdir=test

PG_CONFIG = pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)
