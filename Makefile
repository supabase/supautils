MODULE_big = supautils
SOURCES = src/supautils.c
OBJS = src/supautils.o

PG_CONFIG = pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)
