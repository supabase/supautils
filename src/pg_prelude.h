// pragmas needed to pass compiling with -Wextra
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wsign-compare"

#include <postgres.h>

#include <access/xact.h>
#include <catalog/namespace.h>
#include <catalog/pg_authid.h>
#include <catalog/pg_collation_d.h>
#include <catalog/pg_proc.h>
#include <commands/defrem.h>
#include <commands/event_trigger.h>
#include <commands/publicationcmds.h>
#include <common/jsonapi.h>
#include <executor/spi.h>
#include <fmgr.h>
#include <miscadmin.h>
#include <nodes/makefuncs.h>
#include <nodes/pg_list.h>
#include <nodes/value.h>
#include <parser/parse_func.h>
#include <tcop/utility.h>
#include <tsearch/ts_locale.h>
#include <utils/acl.h>
#include <utils/builtins.h>
#include <utils/fmgrprotos.h>
#include <utils/formatting.h>
#include <utils/guc.h>
#include <utils/guc_tables.h>
#include <utils/json.h>
#include <utils/jsonb.h>
#include <utils/jsonfuncs.h>
#include <utils/lsyscache.h>
#include <utils/memutils.h>
#include <utils/regproc.h>
#include <utils/snapmgr.h>
#include <utils/syscache.h>
#include <utils/varlena.h>

#pragma GCC diagnostic pop
