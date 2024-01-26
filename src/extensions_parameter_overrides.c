#include <postgres.h>

#include <common/jsonapi.h>
#include <miscadmin.h>
#include <tsearch/ts_locale.h>
#include <utils/builtins.h>
#include <utils/json.h>
#include <utils/jsonb.h>
#include <utils/jsonfuncs.h>
#include <utils/memutils.h>

#include "extensions_parameter_overrides.h"

static void json_array_start(void *state) {
    json_extension_parameter_overrides_parse_state *parse = state;

    parse->state = JEPO_UNEXPECTED_ARRAY;
    parse->error_msg = "unexpected array";
}

static void json_object_start(void *state) {
    json_extension_parameter_overrides_parse_state *parse = state;

    switch (parse->state) {
    case JEPO_EXPECT_TOPLEVEL_START:
        parse->state = JEPO_EXPECT_TOPLEVEL_FIELD;
        break;
    case JEPO_EXPECT_SCHEMA:
        parse->error_msg = "unexpected object for schema, expected a value";
        parse->state = JEPO_UNEXPECTED_OBJECT;
        break;
    default:
        break;
    }
}

static void json_object_end(void *state) {
    json_extension_parameter_overrides_parse_state *parse = state;

    switch (parse->state) {
    case JEPO_EXPECT_PARAMETER_OVERRIDES_START:
        parse->state = JEPO_EXPECT_TOPLEVEL_FIELD;
        (parse->total_epos)++;
        break;
    default:
        break;
    }
}

static void json_object_field_start(void *state, char *fname, bool isnull) {
    json_extension_parameter_overrides_parse_state *parse = state;
    extension_parameter_overrides *x = &parse->epos[parse->total_epos];

    switch (parse->state) {
    case JEPO_EXPECT_TOPLEVEL_FIELD:
        x->name = MemoryContextStrdup(TopMemoryContext, fname);
        parse->state = JEPO_EXPECT_PARAMETER_OVERRIDES_START;
        break;

    case JEPO_EXPECT_PARAMETER_OVERRIDES_START:
        if (strcmp(fname, "schema") == 0)
            parse->state = JEPO_EXPECT_SCHEMA;
        else {
            parse->state = JEPO_UNEXPECTED_FIELD;
            parse->error_msg = "unexpected field, only schema is allowed";
        }
        break;

    default:
        break;
    }
}

static void json_scalar(void *state, char *token, JsonTokenType tokentype) {
    json_extension_parameter_overrides_parse_state *parse = state;
    extension_parameter_overrides *x = &parse->epos[parse->total_epos];

    switch (parse->state) {
    case JEPO_EXPECT_SCHEMA:
        if (tokentype == JSON_TOKEN_STRING) {
            x->schema = MemoryContextStrdup(TopMemoryContext, token);
            parse->state = JEPO_EXPECT_PARAMETER_OVERRIDES_START;
        } else {
            parse->state = JEPO_UNEXPECTED_SCHEMA_VALUE;
            parse->error_msg = "unexpected schema value, expected a string";
        }
        break;

    case JEPO_EXPECT_TOPLEVEL_START:
        parse->state = JEPO_UNEXPECTED_SCALAR;
        parse->error_msg = "unexpected scalar, expected an object";
        break;

    case JEPO_EXPECT_PARAMETER_OVERRIDES_START:
        parse->state = JEPO_UNEXPECTED_SCALAR;
        parse->error_msg = "unexpected scalar, expected an object";
        break;

    default:
        break;
    }
}

json_extension_parameter_overrides_parse_state
parse_extensions_parameter_overrides(const char *str,
                                     extension_parameter_overrides *epos) {
    JsonLexContext *lex;
    JsonParseErrorType json_error;
    JsonSemAction sem;

    json_extension_parameter_overrides_parse_state state = {
        JEPO_EXPECT_TOPLEVEL_START, NULL, 0, epos};

    lex =
        makeJsonLexContextCstringLen(pstrdup(str), strlen(str), PG_UTF8, true);

    sem.semstate = &state;
    sem.object_start = json_object_start;
    sem.object_end = json_object_end;
    sem.array_start = json_array_start;
    sem.array_end = NULL;
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
