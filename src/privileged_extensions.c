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

// TODO: interpolate extschema, current_role, current_database_owner
static void run_custom_script(const char *filename) {
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

void handle_create_extension(
    void (*process_utility_hook)(PROCESS_UTILITY_PARAMS),
    PROCESS_UTILITY_PARAMS, const char *privileged_extensions,
    const char *privileged_extensions_superuser,
    const char *privileged_extensions_custom_scripts_path) {
    CreateExtensionStmt *stmt = (CreateExtensionStmt *)pstmt->utilityStmt;
    char *filename = (char *)palloc(MAXPGPATH);

    // Run before-create script.
    {
        switch_to_superuser(privileged_extensions_superuser);

        snprintf(filename, MAXPGPATH, "%s/%s/before-create.sql",
                 privileged_extensions_custom_scripts_path, stmt->extname);
        run_custom_script(filename);

        switch_to_original_role();
    }

    // Run `CREATE EXTENSION`.
    if (!superuser() && is_string_in_comma_delimited_string(
                            stmt->extname, privileged_extensions)) {
        switch_to_superuser(privileged_extensions_superuser);

        run_process_utility_hook(process_utility_hook);

        switch_to_original_role();
    } else {
        run_process_utility_hook(process_utility_hook);
    }

    // Run after-create script.
    {
        switch_to_superuser(privileged_extensions_superuser);

        snprintf(filename, MAXPGPATH, "%s/%s/after-create.sql",
                 privileged_extensions_custom_scripts_path, stmt->extname);
        run_custom_script(filename);

        switch_to_original_role();
    }

    pfree(filename);
}

void handle_alter_extension(
    void (*process_utility_hook)(PROCESS_UTILITY_PARAMS),
    PROCESS_UTILITY_PARAMS, const char *privileged_extensions,
    const char *privileged_extensions_superuser) {
    AlterExtensionStmt *stmt = (AlterExtensionStmt *)pstmt->utilityStmt;

    if (is_string_in_comma_delimited_string(stmt->extname,
                                            privileged_extensions)) {
        switch_to_superuser(privileged_extensions_superuser);

        run_process_utility_hook(process_utility_hook);

        switch_to_original_role();
    } else {
        run_process_utility_hook(process_utility_hook);
    }
}

void handle_drop_extension(void (*process_utility_hook)(PROCESS_UTILITY_PARAMS),
                           PROCESS_UTILITY_PARAMS,
                           const char *privileged_extensions,
                           const char *privileged_extensions_superuser) {
    DropStmt *stmt = (DropStmt *)pstmt->utilityStmt;
    bool all_extensions_are_privileged = true;
    ListCell *lc;

    foreach (lc, stmt->objects) {
        char *name = strVal(lfirst(lc));

        if (!is_string_in_comma_delimited_string(name, privileged_extensions)) {
            all_extensions_are_privileged = false;
            break;
        }
    }

    if (all_extensions_are_privileged) {
        switch_to_superuser(privileged_extensions_superuser);

        run_process_utility_hook(process_utility_hook);

        switch_to_original_role();
    } else {
        run_process_utility_hook(process_utility_hook);
    }
}
