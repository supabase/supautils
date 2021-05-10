MODULE_big = supautils
SOURCES = src/supautils.c
OBJS = src/supautils.o
REGRESS = reserved_roles
REGRESS_OPTS = --inputdir=test \
							 --use-existing  \
							 --user=nosuper

PG_CONFIG = pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)
