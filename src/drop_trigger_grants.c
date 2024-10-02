#include <postgres.h>

#include <common/jsonapi.h>
#include <miscadmin.h>
#include <tsearch/ts_locale.h>
#include <utils/builtins.h>
#include <utils/json.h>
#include <utils/jsonb.h>
#include <utils/jsonfuncs.h>
#include <utils/memutils.h>
#include <utils/regproc.h>
#include <utils/varlena.h>

#include "drop_trigger_grants.h"
#include "utils.h"

static JSON_ACTION_RETURN_TYPE json_array_start(void *state) {
    json_drop_trigger_grants_parse_state *parse = state;

    switch (parse->state) {
    case JDTG_EXPECT_TABLES_START:
        parse->state = JDTG_EXPECT_TABLE;
        break;

    case JDTG_EXPECT_TOPLEVEL_START:
        parse->state = JDTG_UNEXPECTED_ARRAY;
        parse->error_msg = "unexpected array";
        break;

    case JDTG_EXPECT_TABLE:
        parse->state = JDTG_UNEXPECTED_ARRAY;
        parse->error_msg = "unexpected array";
        break;

    default:
        break;
    }
    JSON_ACTION_RETURN;
}

static JSON_ACTION_RETURN_TYPE json_array_end(void *state) {
    json_drop_trigger_grants_parse_state *parse = state;

    switch (parse->state) {
    case JDTG_EXPECT_TABLE:
        parse->state = JDTG_EXPECT_TOPLEVEL_FIELD;
        parse->total_dtgs++;
        break;

    default:
        break;
    }
    JSON_ACTION_RETURN;
}

static JSON_ACTION_RETURN_TYPE json_object_start(void *state) {
    json_drop_trigger_grants_parse_state *parse = state;

    switch (parse->state) {
    case JDTG_EXPECT_TOPLEVEL_START:
        parse->state = JDTG_EXPECT_TOPLEVEL_FIELD;
        break;

    case JDTG_EXPECT_TABLES_START:
        parse->error_msg = "unexpected object for tables, expected an array";
        parse->state = JDTG_UNEXPECTED_OBJECT;
        break;

    case JDTG_EXPECT_TABLE:
        parse->error_msg = "unexpected object for table, expected a string";
        parse->state = JDTG_UNEXPECTED_OBJECT;
        break;

    default:
        break;
    }
    JSON_ACTION_RETURN;
}

static JSON_ACTION_RETURN_TYPE json_object_field_start(void *state, char *fname,
                                                       bool isnull) {
    json_drop_trigger_grants_parse_state *parse = state;
    drop_trigger_grants *x = &parse->dtgs[parse->total_dtgs];

    switch (parse->state) {
    case JDTG_EXPECT_TOPLEVEL_FIELD:
        x->role_name = MemoryContextStrdup(TopMemoryContext, fname);
        parse->state = JDTG_EXPECT_TABLES_START;
        break;

    default:
        break;
    }
    JSON_ACTION_RETURN;
}

static JSON_ACTION_RETURN_TYPE json_scalar(void *state, char *token,
                                           JsonTokenType tokentype) {
    json_drop_trigger_grants_parse_state *parse = state;
    drop_trigger_grants *x = &parse->dtgs[parse->total_dtgs];

    switch (parse->state) {
    case JDTG_EXPECT_TABLE:
        if (tokentype == JSON_TOKEN_STRING) {
            x->table_names[x->total_tables] =
                MemoryContextStrdup(TopMemoryContext, token);
            x->total_tables++;
        } else {
            parse->state = JDTG_UNEXPECTED_TABLE_VALUE;
            parse->error_msg = "unexpected table value, expected a string";
        }
        break;

    case JDTG_EXPECT_TOPLEVEL_START:
        parse->state = JDTG_UNEXPECTED_SCALAR;
        parse->error_msg = "unexpected scalar, expected an object";
        break;

    case JDTG_EXPECT_TABLES_START:
        parse->state = JDTG_UNEXPECTED_SCALAR;
        parse->error_msg = "unexpected scalar, expected an array";
        break;

    default:
        break;
    }
    JSON_ACTION_RETURN;
}

json_drop_trigger_grants_parse_state
parse_drop_trigger_grants(const char *str, drop_trigger_grants *dtgs) {
    JsonLexContext *lex;
    JsonParseErrorType json_error;
    JsonSemAction sem;

    json_drop_trigger_grants_parse_state state = {JDTG_EXPECT_TOPLEVEL_START,
                                                  NULL, 0, dtgs};

    lex =
        makeJsonLexContextCstringLen(pstrdup(str), strlen(str), PG_UTF8, true);

    sem.semstate = &state;
    sem.object_start = json_object_start;
    sem.object_end = NULL;
    sem.array_start = json_array_start;
    sem.array_end = json_array_end;
    sem.object_field_start = json_object_field_start;
    sem.object_field_end = NULL;
    sem.array_element_start = NULL;
    sem.array_element_end = NULL;
    sem.scalar = json_scalar;

    json_error = pg_parse_json(lex, &sem);

    if (json_error != JSON_SUCCESS)
        state.error_msg = "invalid json";

    return state;
}

bool is_current_role_granted_table_drop_trigger(const RangeVar *table_range_var,
                                                const drop_trigger_grants *dtgs,
                                                const size_t total_dtgs) {

    Oid target_table_id =
        RangeVarGetRelid(table_range_var, AccessExclusiveLock, false);
    char *current_role_name = GetUserNameFromId(GetUserId(), false);

    for (size_t i = 0; i < total_dtgs; i++) {
        const drop_trigger_grants *dtg = &dtgs[i];

        if (strcmp(dtg->role_name, current_role_name) != 0) {
            continue;
        }

        for (size_t j = 0; j < dtg->total_tables; j++) {
            const char *table_name = dtg->table_names[j];
            List *qual_name_list;
            RangeVar *range_var;
            Oid table_id;
#if PG16_GTE
            qual_name_list = stringToQualifiedNameList(table_name, NULL);
#else
            qual_name_list = stringToQualifiedNameList(table_name);
#endif
            if (qual_name_list == NULL) {
                list_free(qual_name_list);
                continue;
            }

            range_var = makeRangeVarFromNameList(qual_name_list);
            table_id = RangeVarGetRelid(range_var, AccessExclusiveLock, true);
            if (!OidIsValid(table_id)) {
                continue;
            }

            if (table_id == target_table_id) {
                return true;
            }
        }
    }

    return false;
}
