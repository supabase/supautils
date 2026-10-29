OS = $(shell uname -s)

GREP ?= grep
PG_CONFIG = pg_config

# the `-Wno`s quiet C90 warnings
PG_CFLAGS = -std=gnu11 -Wextra -Wall -Werror \
	-Wno-declaration-after-statement \
	-Wno-vla \
	-Wno-long-long

ifeq ($(TEST_CORE), 1)
PG_CFLAGS += -DTEST_CORE=1
endif

ifeq ($(COVERAGE), 1)
PG_CFLAGS += --coverage
endif

MODULE_big = supautils
MODVERSION = 4.0.0

SRC_DIR = src

SRC = $(wildcard src/*.c)

PG_VERSION = $(strip $(shell $(PG_CONFIG) --version | $(GREP) -oP '(?<=PostgreSQL )[0-9]+'))
# 0 is true
PG_EQ15 = $(shell test $(PG_VERSION) -eq 15; echo $$?)
PG_NEQ15 = $(shell test $(PG_VERSION) -ne 15; echo $$?)
PG_GE15 = $(shell test $(PG_VERSION) -ge 15; echo $$?)
PG_LE13 = $(shell test $(PG_VERSION) -le 13; echo $$?)
PG_LT15 = $(shell test $(PG_VERSION) -lt 15; echo $$?)
PG_EQ18 = $(shell test $(PG_VERSION) -eq 18; echo $$?)
PG_NEQ18 = $(shell test $(PG_VERSION) -ne 18; echo $$?)
PG_GE18 = $(shell test $(PG_VERSION) -ge 18; echo $$?)
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

ifneq ($(PG_GE18), 0)
TESTS := $(filter-out test/sql/ge18_%.sql, $(TESTS))
else
TESTS := $(filter-out test/sql/lt18_%.sql, $(TESTS))
endif

ifeq ($(PG_NEQ15), 0)
TESTS := $(filter-out test/sql/eq15_%.sql, $(TESTS))
endif
ifeq ($(PG_NEQ18), 0)
TESTS := $(filter-out test/sql/eq18_%.sql, $(TESTS))
endif

REGRESS = $(patsubst test/sql/%.sql,%,$(TESTS))
REGRESS_OPTS = --use-existing --inputdir=test

GENERATED_OUT = test/expected/role_spec_forms.out test/expected/event_triggers.out test/expected/permission_hints.out test/expected/privileged_role.out test/expected/ge18_supautils_version.out test/expected/lt18_supautils_version.out
EXTRA_CLEAN = $(GENERATED_OUT)

PGXS := $(shell $(PG_CONFIG) --pgxs)

.DEFAULT_GOAL := all

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

PG_CPPFLAGS := $(CPPFLAGS) -DMODVERSION=\"$(MODVERSION)\"

ifdef BUILD_DIR
OBJS = $(patsubst $(SRC_DIR)/%.c, $(BUILD_DIR)/%.o, $(SRC))
else
OBJS = $(patsubst $(SRC_DIR)/%.c, src/%.o, $(SRC)) # if no BUILD_DIR, just build on src so standard PGXS `make` works
endif

build: $(BUILD_DIR)/$(MODULE_big).$(DL_SUFFIX) test/init.conf

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

$(BUILD_DIR)/.gitignore:
	mkdir -p $(BUILD_DIR)
	echo "*" > $(BUILD_DIR)/.gitignore

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c $(BUILD_DIR)/.gitignore
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/$(MODULE_big).$(DL_SUFFIX): $(MODULE_big).$(DL_SUFFIX)
	mv $? $@

include $(PGXS)

.PHONY: test/expected/event_triggers.out
test/expected/event_triggers.out: test/expected/event_triggers.out.in
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

# core's permission errors changed wording in PG16
.PHONY: test/expected/role_spec_forms.out
test/expected/role_spec_forms.out: test/expected/%.out: test/expected/%.out.in
ifeq ($(PG_GE16), 0)
	sed \
		-e '/<\/\?PG_GE_16>/d' \
		-e '/<PG_GE_14>/,/<\/PG_GE_14>/d' \
		$< > $@
else
	sed \
		-e '/<\/\?PG_GE_14>/d' \
		-e '/<PG_GE_16>/,/<\/PG_GE_16>/d' \
		$< > $@
endif

.PHONY: test/expected/permission_hints.out
test/expected/permission_hints.out: test/expected/permission_hints.out.in
ifeq ($(PG_GE18), 0)
	sed \
		-e '/<\/\?PG_GE_15>/d' \
		-e '/<PG_LT_18>/,/<\/PG_LT_18>/d' \
		-e '/<PG_LT_15>/,/<\/PG_LT_15>/d' \
		$? > $@
else ifeq ($(PG_GE15), 0)
	sed \
		-e '/<\/\?PG_GE_15>/d' \
		-e '/<\/\?PG_LT_18>/d' \
		-e '/<PG_LT_15>/,/<\/PG_LT_15>/d' \
		$? > $@
else
	sed \
		-e '/<\/\?PG_LT_15>/d' \
		-e '/<\/\?PG_LT_18>/d' \
		-e '/<PG_GE_15>/,/<\/PG_GE_15>/d' \
		$? > $@
endif

.PHONY: test/expected/privileged_role.out
test/expected/privileged_role.out: test/expected/privileged_role.out.in
ifeq ($(PG_LE13), 0)
	sed \
		-e '/<\/\?PG_LE13>/d' \
		-e '/<PG_GT13>/,/<\/PG_GT13>/d' \
		$? > $@
else
	sed \
		-e '/<\/\?PG_GT13>/d' \
		-e '/<PG_LE13>/,/<\/PG_LE13>/d' \
		$? > $@
endif

test/expected/%_supautils_version.out: test/expected/%_supautils_version.out.in
	sed \
		-e 's/@MODVERSION@/$(MODVERSION)/' \
		$? > $@

# extra dep for target in pgxs.mk
installcheck: $(GENERATED_OUT)

CORE_STAGEDIR = $(BUILD_DIR)/$(notdir $(PG_REGRESS_TESTS))
CORE_VERSION_PATCH_DIR = $(if $(filter 15 16 17 18 19,$(PG_VERSION)),test/core_patches/$(PG_VERSION))
CORE_PATCHES = $(wildcard $(CORE_VERSION_PATCH_DIR)/*.patch)
# src/test/modules/unsafe_tests from the same PG version. Upstream keeps these
# out of installcheck because they change cluster-wide state; our cluster is
# disposable. Staged only when the directory exists.
UNSAFE_TESTS_DIR ?= $(PG_REGRESS_TESTS)/unsafe_tests
UNSAFE_TESTS = rolenames setconfig alter_system_table guc_privs
UNSAFE_PATCHES = $(wildcard $(CORE_VERSION_PATCH_DIR)/unsafe_tests/*.patch)

$(CORE_STAGEDIR)/parallel_schedule: $(CORE_PATCHES)
	rm -rf $(CORE_STAGEDIR)
	mkdir -p $(CORE_STAGEDIR)/sql $(CORE_STAGEDIR)/expected $(CORE_STAGEDIR)/data
	cp -R $(PG_REGRESS_TESTS)/sql/. $(CORE_STAGEDIR)/sql/
	cp -R $(PG_REGRESS_TESTS)/expected/. $(CORE_STAGEDIR)/expected/
	cp -R $(PG_REGRESS_TESTS)/data/. $(CORE_STAGEDIR)/data/
	cp $(PG_REGRESS_TESTS)/parallel_schedule $(CORE_STAGEDIR)/
	@set -e; \
	for patch in $(CORE_PATCHES); do \
		patch -d $(CORE_STAGEDIR) -p1 < $$patch; \
	done
	@set -e; \
	if [ -d $(UNSAFE_TESTS_DIR) ]; then \
		chmod -R u+w $(CORE_STAGEDIR); \
		cp $(UNSAFE_TESTS_DIR)/sql/*.sql $(CORE_STAGEDIR)/sql/; \
		cp $(UNSAFE_TESTS_DIR)/expected/*.out $(CORE_STAGEDIR)/expected/; \
		for patch in $(UNSAFE_PATCHES); do \
			patch -d $(CORE_STAGEDIR) -p1 < $$patch; \
		done; \
		for t in $(UNSAFE_TESTS); do echo "test: $$t" >> $(CORE_STAGEDIR)/parallel_schedule; done; \
	fi

.PHONY: test-core
test-core: $(CORE_STAGEDIR)/parallel_schedule
	$(pg_regress_installcheck) --dlpath=$(PG_REGRESS_TESTS)/lib --inputdir=$(CORE_STAGEDIR) --schedule=$(CORE_STAGEDIR)/parallel_schedule --dbname=regression --create-role=regress_authenticated_user_sr --create-role=regress_authenticated_user_ssa

.PHONY: test-startup
test-startup:
	bash test/startup.sh

.PHONY: test
test:
	make installcheck
	make test-startup
