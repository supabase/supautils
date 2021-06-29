MODULE_big = supautils
OBJS = src/supautils.o
REGRESS = reserved_roles reserved_memberships
REGRESS_OPTS = --inputdir=test

PG_CONFIG = pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)
