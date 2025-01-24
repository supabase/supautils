#include "pg_prelude.h"

#ifdef __linux__
#include <sys/sysinfo.h>
#endif
#include <sys/statvfs.h>
#include <errno.h>

#include "constrained_extensions.h"
#include "utils.h"

static JSON_ACTION_RETURN_TYPE
json_array_start(void *state)
{
    json_constrained_extension_parse_state *parse = state;

    parse->state = JCE_UNEXPECTED_ARRAY;
    parse->error_msg = "unexpected array";
    JSON_ACTION_RETURN;
}

static JSON_ACTION_RETURN_TYPE
json_object_start(void *state)
{
    json_constrained_extension_parse_state *parse = state;

    switch (parse->state) {
    case JCE_EXPECT_TOPLEVEL_START:
        parse->state = JCE_EXPECT_TOPLEVEL_FIELD;
        break;
    case JCE_EXPECT_CPU:
    case JCE_EXPECT_MEM:
    case JCE_EXPECT_DISK:
        parse->error_msg = "unexpected object for cpu, mem or disk, expected a value";
        parse->state = JCE_UNEXPECTED_OBJECT;
        break;
    default:
        break;
    }
    JSON_ACTION_RETURN;
}

static JSON_ACTION_RETURN_TYPE
json_object_end(void *state)
{
    json_constrained_extension_parse_state *parse = state;

    switch (parse->state) {
    case JCE_EXPECT_CONSTRAINTS_START:
        parse->state = JCE_EXPECT_TOPLEVEL_FIELD;
        (parse->total_cexts)++;
        break;
    default:
        break;
    }
    JSON_ACTION_RETURN;
}

static JSON_ACTION_RETURN_TYPE
json_object_field_start(void *state, char *fname, __attribute__ ((unused)) bool isnull)
{
    json_constrained_extension_parse_state *parse = state;
    constrained_extension *x = &parse->cexts[parse->total_cexts];

    switch (parse->state) {
    case JCE_EXPECT_TOPLEVEL_FIELD:
        x->name = MemoryContextStrdup(TopMemoryContext, fname);
        parse->state = JCE_EXPECT_CONSTRAINTS_START;
        break;

    case JCE_EXPECT_CONSTRAINTS_START:
        if (strcmp(fname, "cpu") == 0)
            parse->state = JCE_EXPECT_CPU;
        else if (strcmp(fname, "mem") == 0)
            parse->state = JCE_EXPECT_MEM;
        else if (strcmp(fname, "disk") == 0)
            parse->state = JCE_EXPECT_DISK;
        else {
            parse->state = JCE_UNEXPECTED_FIELD;
            parse->error_msg = "unexpected field, only cpu, mem or disk are allowed";
        }
        break;

    default:
        break;
    }
    JSON_ACTION_RETURN;
}

static JSON_ACTION_RETURN_TYPE
json_scalar(void *state, char *token, JsonTokenType tokentype)
{
    json_constrained_extension_parse_state *parse = state;
    constrained_extension *x = &parse->cexts[parse->total_cexts];

    switch (parse->state) {
    case JCE_EXPECT_CPU:
        if(tokentype == JSON_TOKEN_NUMBER){
            x->cpu = atoi(token);
            parse->state = JCE_EXPECT_CONSTRAINTS_START;
        } else {
            parse->state = JCE_UNEXPECTED_CPU_VALUE;
            parse->error_msg = "unexpected cpu value, expected a number";
        }
        break;

    case JCE_EXPECT_MEM:
        if(tokentype == JSON_TOKEN_STRING){
            x->mem = DatumGetInt64(DirectFunctionCall1(pg_size_bytes, CStringGetTextDatum(token)));
            parse->state = JCE_EXPECT_CONSTRAINTS_START;
        } else {
            parse->state = JCE_UNEXPECTED_MEM_VALUE;
            parse->error_msg = "unexpected mem value, expected a string with bytes in human-readable format (as returned by pg_size_pretty)";
        }
        break;

    case JCE_EXPECT_DISK:
        if(tokentype == JSON_TOKEN_STRING){
            x->disk = DatumGetInt64(DirectFunctionCall1(pg_size_bytes, CStringGetTextDatum(token)));
            parse->state = JCE_EXPECT_CONSTRAINTS_START;
        } else {
            parse->state = JCE_UNEXPECTED_DISK_VALUE;
            parse->error_msg = "unexpected disk value, expected a string with bytes in human-readable format (as returned by pg_size_pretty)";
        }
        break;

    case JCE_EXPECT_TOPLEVEL_START:
        parse->state = JCE_UNEXPECTED_SCALAR;
        parse->error_msg = "unexpected scalar, expected an object";
        break;

    case JCE_EXPECT_CONSTRAINTS_START:
        parse->state = JCE_UNEXPECTED_SCALAR;
        parse->error_msg = "unexpected scalar, expected an object";
        break;

    default:
        break;
    }
    JSON_ACTION_RETURN;
}

json_constrained_extension_parse_state
parse_constrained_extensions(
    const char *str,
    constrained_extension *cexts
){
    JsonLexContext *lex;
    JsonParseErrorType json_error;
    JsonSemAction sem;

    json_constrained_extension_parse_state state = {JCE_EXPECT_TOPLEVEL_START, NULL, 0, cexts};

    lex = NEW_JSON_LEX_CONTEXT_CSTRING_LEN(pstrdup(str), strlen(str), PG_UTF8, true);

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

#define ERROR_HINT "upgrade to an instance with higher resources"

// implementation is Linux specific
// * the CPUs obtained is the equivalent of `lscpu | grep 'CPU(s)'`
// * the memory obtained is the equivalent of the total on `free -b`
// * the disk obtained is the equivalent of the available on `df -B1`
void constrain_extension(
    const char* name,
    constrained_extension *cexts,
    const size_t total_cexts
){
#ifdef __linux__
    struct sysinfo info = {};
#endif
    struct statvfs fsdata = {};

#ifdef __linux__
    if(sysinfo(&info) < 0){
        int save_errno = errno;
        ereport(ERROR, errmsg("sysinfo call failed: %s", strerror(save_errno)));
    }
#endif

    if (statvfs(DataDir, &fsdata) < 0){
        int save_errno = errno;
        ereport(ERROR, errmsg("statvfs call failed: %s", strerror(save_errno)));
    }

    for(size_t i = 0; i < total_cexts; i++){
        if (strcmp(name, cexts[i].name) == 0) {
#ifdef __linux__
            if(cexts[i].cpu != 0 && cexts[i].cpu > get_nprocs())
                ereport(ERROR,
                    errdetail("required CPUs: %d", cexts[i].cpu),
                    errhint(ERROR_HINT),
                    errmsg("not enough CPUs for using this extension")
                );
            if(cexts[i].mem != 0 && cexts[i].mem > info.totalram){
                char *pretty_size = text_to_cstring(DatumGetTextPP(DirectFunctionCall1(pg_size_pretty, Int64GetDatum(cexts[i].mem))));
                ereport(ERROR,
                    errdetail("required memory: %s", pretty_size),
                    errhint(ERROR_HINT),
                    errmsg("not enough memory for using this extension")
                );
            }
#endif
            if(cexts[i].disk != 0 && cexts[i].disk > (size_t) (fsdata.f_bfree * fsdata.f_bsize)){
                char *pretty_size = text_to_cstring(DatumGetTextPP(DirectFunctionCall1(pg_size_pretty, Int64GetDatum(cexts[i].disk))));
                ereport(ERROR,
                    errdetail("required free disk space: %s", pretty_size),
                    errhint(ERROR_HINT),
                    errmsg("not enough free disk space for using this extension")
                );
            }
        }
    }
}
