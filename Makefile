OS = $(shell uname -s)

GREP ?= grep
PG_CONFIG = pg_config

# the `-Wno`s quiet C90 warnings
PG_CFLAGS = -std=c11 -Wextra -Wall -Werror \
	-Wno-declaration-after-statement \
	-Wno-vla \
	-Wno-long-long

ifeq ($(TEST), 1)
PG_CFLAGS += -DTEST
endif

ifeq ($(COVERAGE), 1)
PG_CFLAGS += --coverage
endif

EXTENSION = supautils
MODULE_big = $(EXTENSION)

SRC_DIR = src
BUILD_DIR ?= build

SRC = $(wildcard src/*.c)
OBJS = $(patsubst $(SRC_DIR)/%.c, $(BUILD_DIR)/%.o, $(SRC))

PG_VERSION = $(strip $(shell $(PG_CONFIG) --version | $(GREP) -oP '(?<=PostgreSQL )[0-9]+'))
# 0 is true
PG_EQ15 = $(shell test $(PG_VERSION) -eq 15; echo $$?)
PG_GE16 = $(shell test $(PG_VERSION) -ge 16; echo $$?)
PG_GE14 = $(shell test $(PG_VERSION) -ge 14; echo $$?)
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

GENERATED_OUT = test/expected/event_triggers.out
EXTRA_CLEAN = $(GENERATED_OUT)

PGXS := $(shell $(PG_CONFIG) --pgxs)

ifeq ($(OS), Linux)
  DL_SUFFIX=so
else ifeq ($(OS), Darwin)
  ifeq ($(PG_GE16), 0)
    DL_SUFFIX=dylib
  else
    DL_SUFFIX=so
  endif
else
  DL_SUFFIX=dylib
endif

build: $(BUILD_DIR)/$(EXTENSION).$(DL_SUFFIX) test/init.conf

.PHONY: test/init.conf
test/init.conf: test/init.conf.in
ifeq ($(PG_EQ15), 0)
	sed \
		-e '/<\/\?PG_EQ_15>/d' \
		$? > $@
else
	sed \
		-e '/<PG_EQ_15>/,/<\/PG_EQ_15>/d' \
		$? > $@
endif

PG_CPPFLAGS := $(CPPFLAGS) -DTEST=1

$(BUILD_DIR)/.gitignore:
	mkdir -p $(BUILD_DIR)
	echo "*" > $(BUILD_DIR)/.gitignore

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c $(BUILD_DIR)/.gitignore
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/$(EXTENSION).$(DL_SUFFIX): $(EXTENSION).$(DL_SUFFIX)
	mv $? $@

include $(PGXS)

.PHONY: $(GENERATED_OUT)
$(GENERATED_OUT): $(GENERATED_OUT).in
ifeq ($(PG_GE16), 0)
	sed \
		-e '/<\/\?PG_GE_16>/d' \
		-e '/<PG_GE_13>/,/<\/PG_GE_13>/d' \
		-e '/<PG_GE_14>/,/<\/PG_GE_14>/d' \
		$? > $@
else ifeq ($(PG_GE14), 0)
	sed \
		-e '/<\/\?PG_GE_14>/d' \
		-e '/<PG_GE_13>/,/<\/PG_GE_13>/d' \
		-e '/<PG_GE_16>/,/<\/PG_GE_16>/d' \
		$? > $@
else
	sed \
		-e '/<\/\?PG_GE_13>/d' \
		-e '/<PG_GE_14>/,/<\/PG_GE_14>/d' \
		-e '/<PG_GE_16>/,/<\/PG_GE_16>/d' \
		$? > $@
endif

# extra dep for target in pgxs.mk
installcheck: $(GENERATED_OUT)

.PHONY: test
test:
	make installcheck
