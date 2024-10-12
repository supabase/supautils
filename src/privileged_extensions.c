#include <postgres.h>

#include <catalog/namespace.h>
#include <catalog/pg_authid.h>
#include <catalog/pg_collation.h>
#include <catalog/pg_type.h>
#include <commands/defrem.h>
#include <executor/spi.h>
#include <miscadmin.h>
#include <nodes/makefuncs.h>
#include <nodes/pg_list.h>
#include <storage/fd.h>
#include <utils/acl.h>
#include <utils/builtins.h>
#include <utils/guc.h>
#include <utils/lsyscache.h>
#include <utils/snapmgr.h>
#include <utils/varlena.h>

#include "extensions_parameter_overrides.h"
#include "privileged_extensions.h"
#include "utils.h"

// Prevent recursively running custom scripts
static bool running_custom_script = false;

static void run_custom_script(const char *filename, const char *extname,
                              const char *extschema, const char *extversion,
                              bool extcascade) {
    if (running_custom_script) {
        return;
    }
    running_custom_script = true;
    PushActiveSnapshot(GetTransactionSnapshot());
    SPI_connect();
    {
        char *sql_tmp01 = "do $$\n"
                          "begin\n"
                          "  execute\n"
                          "    replace(\n"
                          "      replace(\n"
                          "        replace(\n"
                          "          replace(\n"
                          "            pg_read_file(\n";
        char *sql_tmp02 = quote_literal_cstr(filename);
        char *sql_tmp03 = "            ),\n"
                          "            '@extname@', ";
        char *sql_tmp04 = quote_literal_cstr(quote_literal_cstr(extname));
        char *sql_tmp05 = "          ),\n"
                          "          '@extschema@', ";
        char *sql_tmp06 =
            extschema == NULL
                ? "'null'"
                : quote_literal_cstr(quote_literal_cstr(extschema));
        char *sql_tmp07 = "        ),\n"
                          "        '@extversion@', ";
        char *sql_tmp08 =
            extversion == NULL
                ? "'null'"
                : quote_literal_cstr(quote_literal_cstr(extversion));
        char *sql_tmp09 = "      ), "
                          "    '@extcascade@', ";
        char *sql_tmp10 = extcascade ? "'true'" : "'false'";
        char *sql_tmp11 = "    );\n"
                          "exception\n"
                          "  when undefined_file then\n"
                          "    -- skip\n"
                          "end\n"
                          "$$;";
        size_t sql_len =
            strlen(sql_tmp01) + strlen(sql_tmp02) + strlen(sql_tmp03) +
            strlen(sql_tmp04) + strlen(sql_tmp05) + strlen(sql_tmp06) +
            strlen(sql_tmp07) + strlen(sql_tmp08) + strlen(sql_tmp09) +
            strlen(sql_tmp10) + strlen(sql_tmp11);
        char *sql = (char *)palloc(sql_len);
        int rc;

        snprintf(sql, sql_len, "%s%s%s%s%s%s%s%s%s%s%s", sql_tmp01, sql_tmp02,
                 sql_tmp03, sql_tmp04, sql_tmp05, sql_tmp06, sql_tmp07,
                 sql_tmp08, sql_tmp09, sql_tmp10, sql_tmp11);

        rc = SPI_execute(sql, false, 0);
        if (rc != SPI_OK_UTILITY) {
            elog(ERROR, "SPI_execute failed with error code %d", rc);
        }
    }
    SPI_finish();
    PopActiveSnapshot();
    running_custom_script = false;
}

void handle_create_extension(
    void (*process_utility_hook)(PROCESS_UTILITY_PARAMS),
    PROCESS_UTILITY_PARAMS, const char *privileged_extensions,
    const char *privileged_extensions_superuser,
    const char *privileged_extensions_custom_scripts_path,
    const extension_parameter_overrides *epos, const size_t total_epos) {
    CreateExtensionStmt *stmt = (CreateExtensionStmt *)pstmt->utilityStmt;
    char *filename = (char *)palloc(MAXPGPATH);

    // Run global before-create script.
    {
        DefElem *d_schema = NULL;
        DefElem *d_new_version = NULL;
        DefElem *d_cascade = NULL;
        char *extschema = NULL;
        char *extversion = NULL;
        bool extcascade = false;
        ListCell *option_cell = NULL;
        bool already_switched_to_superuser = false;

        foreach (option_cell, stmt->options) {
            DefElem *defel = (DefElem *)lfirst(option_cell);

            if (strcmp(defel->defname, "schema") == 0) {
                d_schema = defel;
                extschema = defGetString(d_schema);
            } else if (strcmp(defel->defname, "new_version") == 0) {
                d_new_version = defel;
                extversion = defGetString(d_new_version);
            } else if (strcmp(defel->defname, "cascade") == 0) {
                d_cascade = defel;
                extcascade = defGetBoolean(d_cascade);
            }
        }

        switch_to_superuser(privileged_extensions_superuser,
                            &already_switched_to_superuser);

        snprintf(filename, MAXPGPATH, "%s/before-create.sql",
                 privileged_extensions_custom_scripts_path);
        run_custom_script(filename, stmt->extname, extschema, extversion,
                          extcascade);

        if (!already_switched_to_superuser) {
            switch_to_original_role();
        }
    }

    // Run per-extension before-create script.
    {
        DefElem *d_schema = NULL;
        DefElem *d_new_version = NULL;
        DefElem *d_cascade = NULL;
        char *extschema = NULL;
        char *extversion = NULL;
        bool extcascade = false;
        ListCell *option_cell = NULL;
        bool already_switched_to_superuser = false;

        foreach (option_cell, stmt->options) {
            DefElem *defel = (DefElem *)lfirst(option_cell);

            if (strcmp(defel->defname, "schema") == 0) {
                d_schema = defel;
                extschema = defGetString(d_schema);
            } else if (strcmp(defel->defname, "new_version") == 0) {
                d_new_version = defel;
                extversion = defGetString(d_new_version);
            } else if (strcmp(defel->defname, "cascade") == 0) {
                d_cascade = defel;
                extcascade = defGetBoolean(d_cascade);
            }
        }

        switch_to_superuser(privileged_extensions_superuser,
                            &already_switched_to_superuser);

        snprintf(filename, MAXPGPATH, "%s/%s/before-create.sql",
                 privileged_extensions_custom_scripts_path, stmt->extname);
        run_custom_script(filename, stmt->extname, extschema, extversion,
                          extcascade);

        if (!already_switched_to_superuser) {
            switch_to_original_role();
        }
    }

    // Apply overrides.
    for (size_t i = 0; i < total_epos; i++) {
        if (strcmp(epos[i].name, stmt->extname) == 0) {
            const extension_parameter_overrides *epo = &epos[i];
            DefElem *schema_option = NULL;
            DefElem *schema_override_option = NULL;
            ListCell *option_cell;

            if (epo->schema != NULL) {
                Node *schema_node = (Node *)makeString(pstrdup(epo->schema));
                schema_override_option = makeDefElem("schema", schema_node, -1);
            }

            foreach (option_cell, stmt->options) {
                DefElem *defel = (DefElem *)lfirst(option_cell);

                if (strcmp(defel->defname, "schema") == 0) {
                    if (schema_option != NULL) {
                        ereport(ERROR,
                                (errcode(ERRCODE_SYNTAX_ERROR),
                                 errmsg("conflicting or redundant options")));
                    }
                    schema_option = defel;
                }
            }

            if (schema_override_option != NULL) {
                if (schema_option != NULL) {
                    stmt->options =
                        list_delete_ptr(stmt->options, schema_option);
                }
                stmt->options = lappend(stmt->options, schema_override_option);
            }
        }
    }

    // Run `CREATE EXTENSION`.
    if (is_string_in_comma_delimited_string(stmt->extname,
                                            privileged_extensions)) {
        bool already_switched_to_superuser = false;
        switch_to_superuser(privileged_extensions_superuser,
                            &already_switched_to_superuser);

        run_process_utility_hook(process_utility_hook);

        if (!already_switched_to_superuser) {
            switch_to_original_role();
        }
    } else {
        run_process_utility_hook(process_utility_hook);
    }

    // Run per-extension after-create script.
    {
        DefElem *d_schema = NULL;
        DefElem *d_new_version = NULL;
        DefElem *d_cascade = NULL;
        char *extschema = NULL;
        char *extversion = NULL;
        bool extcascade = false;
        ListCell *option_cell = NULL;
        bool already_switched_to_superuser = false;

        foreach (option_cell, stmt->options) {
            DefElem *defel = (DefElem *)lfirst(option_cell);

            if (strcmp(defel->defname, "schema") == 0) {
                d_schema = defel;
                extschema = defGetString(d_schema);
            } else if (strcmp(defel->defname, "new_version") == 0) {
                d_new_version = defel;
                extversion = defGetString(d_new_version);
            } else if (strcmp(defel->defname, "cascade") == 0) {
                d_cascade = defel;
                extcascade = defGetBoolean(d_cascade);
            }
        }

        switch_to_superuser(privileged_extensions_superuser,
                            &already_switched_to_superuser);

        snprintf(filename, MAXPGPATH, "%s/%s/after-create.sql",
                 privileged_extensions_custom_scripts_path, stmt->extname);
        run_custom_script(filename, stmt->extname, extschema, extversion,
                          extcascade);

        if (!already_switched_to_superuser) {
            switch_to_original_role();
        }
    }

    pfree(filename);
}

void handle_alter_extension(
    void (*process_utility_hook)(PROCESS_UTILITY_PARAMS),
    PROCESS_UTILITY_PARAMS,
    const char *extname, const char *privileged_extensions,
    const char *privileged_extensions_superuser) {

    if (is_string_in_comma_delimited_string(extname,
                                            privileged_extensions)) {
        bool already_switched_to_superuser = false;
        switch_to_superuser(privileged_extensions_superuser,
                            &already_switched_to_superuser);

        run_process_utility_hook(process_utility_hook);

        if (!already_switched_to_superuser) {
            switch_to_original_role();
        }
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
        bool already_switched_to_superuser = false;
        switch_to_superuser(privileged_extensions_superuser,
                            &already_switched_to_superuser);

        run_process_utility_hook(process_utility_hook);

        if (!already_switched_to_superuser) {
            switch_to_original_role();
        }
    } else {
        run_process_utility_hook(process_utility_hook);
    }
}
