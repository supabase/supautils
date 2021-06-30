EXTENSION = supautils
DATA = src/supautils--0.1.0.sql

MODULE_big = supautils
OBJS = src/supautils.o
REGRESS = reserved_roles reserved_memberships supautils
REGRESS_OPTS = --inputdir=test

PG_CONFIG = pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)
