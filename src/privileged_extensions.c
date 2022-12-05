#include <postgres.h>

#include <catalog/namespace.h>
#include <catalog/pg_authid.h>
#include <catalog/pg_collation.h>
#include <catalog/pg_type.h>
#include <executor/spi.h>
#include <miscadmin.h>
#include <nodes/pg_list.h>
#include <storage/fd.h>
#include <utils/acl.h>
#include <utils/builtins.h>
#include <utils/guc.h>
#include <utils/lsyscache.h>
#include <utils/snapmgr.h>
#include <utils/varlena.h>

#include "privileged_extensions.h"
#include "utils.h"

static bool is_extension_privileged(char *name, char *privileged_extensions) {
    bool extension_is_privileged = false;
    List *privileged_extensions_list;
    ListCell *lc;

    SplitIdentifierString(pstrdup(privileged_extensions), ',',
                          &privileged_extensions_list);

    foreach (lc, privileged_extensions_list) {
        char *privileged_extension = (char *)lfirst(lc);

        if (strcmp(name, privileged_extension) == 0) {
            extension_is_privileged = true;
            break;
        }
    }
    list_free(privileged_extensions_list);

    return extension_is_privileged;
}

// TODO: interpolate extschema, current_role, current_database_owner
static void run_custom_script(char *filename) {
    PushActiveSnapshot(GetTransactionSnapshot());
    SPI_connect();
    {
        char *sql_tmp1 = "do $$\n"
                         "begin\n"
                         "  execute pg_read_file(";
        char *sql_tmp2 = quote_literal_cstr(filename);
        char *sql_tmp3 = ");\n"
                         "exception\n"
                         "  when undefined_file then\n"
                         "    -- skip\n"
                         "end\n"
                         "$$;";
        size_t sql_len = strlen(sql_tmp1) + strlen(sql_tmp2) + strlen(sql_tmp3);
        char *sql = (char *)palloc(sql_len);
        int rc;

        snprintf(sql, sql_len, "%s%s%s", sql_tmp1, sql_tmp2, sql_tmp3);

        rc = SPI_execute(sql, false, 0);
        if (rc != SPI_OK_UTILITY) {
            elog(ERROR, "SPI_execute failed with error code %d", rc);
        }

        pfree(sql_tmp2);
        pfree(sql);
    }
    SPI_finish();
    PopActiveSnapshot();
}

static void run_process_utility_hook_as_superuser(
    void (*process_utility_hook)(PROCESS_UTILITY_PARAMS),
    PROCESS_UTILITY_PARAMS, char *privileged_extensions_superuser,
    char *privileged_extensions_custom_scripts_path) {
    switch_to_superuser(privileged_extensions_superuser);

    if (IsA(pstmt->utilityStmt, CreateExtensionStmt)) {
        CreateExtensionStmt *stmt = (CreateExtensionStmt *)pstmt->utilityStmt;
        char *filename = (char *)palloc(MAXPGPATH);

        snprintf(filename, MAXPGPATH, "%s/%s/before-create.sql",
                 privileged_extensions_custom_scripts_path, stmt->extname);
        run_custom_script(filename);

        pfree(filename);
    }

    run_process_utility_hook(process_utility_hook);

    if (IsA(pstmt->utilityStmt, CreateExtensionStmt)) {
        CreateExtensionStmt *stmt = (CreateExtensionStmt *)pstmt->utilityStmt;
        char *filename = (char *)palloc(MAXPGPATH);

        snprintf(filename, MAXPGPATH, "%s/%s/after-create.sql",
                 privileged_extensions_custom_scripts_path, stmt->extname);
        run_custom_script(filename);

        pfree(filename);
    }

    switch_to_original_role();
}

void handle_create_extension(
    void (*process_utility_hook)(PROCESS_UTILITY_PARAMS),
    PROCESS_UTILITY_PARAMS, char *privileged_extensions,
    char *privileged_extensions_superuser,
    char *privileged_extensions_custom_scripts_path) {
    CreateExtensionStmt *stmt = (CreateExtensionStmt *)pstmt->utilityStmt;

    if (is_extension_privileged(stmt->extname, privileged_extensions)) {
        run_process_utility_hook_as_superuser(
            process_utility_hook, PROCESS_UTILITY_ARGS,
            privileged_extensions_superuser,
            privileged_extensions_custom_scripts_path);
    } else {
        run_process_utility_hook(process_utility_hook);
    }
}

void handle_alter_extension(
    void (*process_utility_hook)(PROCESS_UTILITY_PARAMS),
    PROCESS_UTILITY_PARAMS, char *privileged_extensions,
    char *privileged_extensions_superuser,
    char *privileged_extensions_custom_scripts_path) {
    AlterExtensionStmt *stmt = (AlterExtensionStmt *)pstmt->utilityStmt;

    if (is_extension_privileged(stmt->extname, privileged_extensions)) {
        run_process_utility_hook_as_superuser(
            process_utility_hook, PROCESS_UTILITY_ARGS,
            privileged_extensions_superuser,
            privileged_extensions_custom_scripts_path);
    } else {
        run_process_utility_hook(process_utility_hook);
    }
}

void handle_drop_extension(void (*process_utility_hook)(PROCESS_UTILITY_PARAMS),
                           PROCESS_UTILITY_PARAMS, char *privileged_extensions,
                           char *privileged_extensions_superuser,
                           char *privileged_extensions_custom_scripts_path) {
    DropStmt *stmt = (DropStmt *)pstmt->utilityStmt;
    bool all_extensions_are_privileged = true;
    ListCell *lc;

    foreach (lc, stmt->objects) {
        char *name = strVal(lfirst(lc));

        if (!is_extension_privileged(name, privileged_extensions)) {
            all_extensions_are_privileged = false;
            break;
        }
    }

    if (all_extensions_are_privileged) {
        run_process_utility_hook_as_superuser(
            process_utility_hook, PROCESS_UTILITY_ARGS,
            privileged_extensions_superuser,
            privileged_extensions_custom_scripts_path);
    } else {
        run_process_utility_hook(process_utility_hook);
    }
}
