#include "pg_prelude.h"
#include "extensions_parameter_overrides.h"
#include "privileged_extensions.h"
#include "utils.h"

// Prevent recursively running custom scripts
static bool running_custom_script = false;

// This produces a char surrounded by a triple single quote like '''x'''
// This is so when it gets interpreted by SQL it converts to a single quote surround: 'x'
// To see an example, do `select 'x';` vs `select '''x''';` on psql.
static char *sql_literal(const char *str){
    return str == NULL?
        "'null'": // also handle the NULL cstr case
        quote_literal_cstr(quote_literal_cstr(str));
}

static void run_custom_script(const char *filename, const char *extname,
                              const char *extschema, const char *extversion,
                              bool extcascade) {
    if (running_custom_script) {
        return;
    }
    running_custom_script = true;

    static const char sql_replace_template[] = "\
    do $_$\
    begin\
      execute replace(replace(replace(replace(\
            pg_read_file(%s)\
          , '@extname@', %s)\
          , '@extschema@', %s)\
          , '@extversion@', %s)\
          , '@extcascade@', %s);\
    exception\
      when undefined_file then\
        null;\
    end; $_$";

    static const size_t max_sql_len
        = sizeof (sql_replace_template)
        + MAXPGPATH // max size of a file path
        + 3 * (NAMEDATALEN + 6) // 3 *(identifier + 6 single quotes of the SQL literal, see sql_literal)
        + sizeof ("false") // max size of a bool string value
        ;

    char sql[max_sql_len];

    snprintf(sql,
            max_sql_len,
            sql_replace_template,
            quote_literal_cstr(filename),
            sql_literal(extname),
            sql_literal(extschema),
            sql_literal(extversion),
            extcascade?"'true'":"'false'");

    PushActiveSnapshot(GetTransactionSnapshot());
    SPI_connect();

    int rc = SPI_execute(sql, false, 0);
    if (rc != SPI_OK_UTILITY) {
        elog(ERROR, "SPI_execute failed with error code %d", rc);
    }
    SPI_finish();
    PopActiveSnapshot();
    running_custom_script = false;
}

void handle_create_extension(
    void (*process_utility_hook)(PROCESS_UTILITY_PARAMS),
    PROCESS_UTILITY_PARAMS, const char *privileged_extensions,
    const char *superuser,
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

        switch_to_superuser(superuser,
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

        switch_to_superuser(superuser,
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
        switch_to_superuser(superuser,
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

        switch_to_superuser(superuser,
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

bool all_extensions_are_privileged(List *objects, const char *privileged_extensions){
  ListCell *lc;

  if(privileged_extensions == NULL) return false;

  foreach (lc, objects) {
      char *name = strVal(lfirst(lc));

      if (!is_string_in_comma_delimited_string(name, privileged_extensions)) {
          return false;
      }
  }

  return true;
}

bool is_extension_privileged(const char *extname, const char *privileged_extensions){
  if(privileged_extensions == NULL) return false;

  return is_string_in_comma_delimited_string(extname, privileged_extensions);
}
