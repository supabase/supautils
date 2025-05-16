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

void run_global_before_create_script(char *extname, List *options, const char *privileged_extensions_custom_scripts_path){
    DefElem *d_schema = NULL, *d_new_version = NULL, *d_cascade = NULL;
    char *extschema = NULL, *extversion = NULL;
    bool extcascade = false;
    char filename[MAXPGPATH];

    ListCell *option_cell = NULL;

    foreach (option_cell, options) {
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

    snprintf(filename, MAXPGPATH, "%s/before-create.sql",
             privileged_extensions_custom_scripts_path);
    run_custom_script(filename, extname, extschema, extversion,
                      extcascade);
}

void run_ext_before_create_script(char *extname, List *options, const char *privileged_extensions_custom_scripts_path){
    DefElem *d_schema = NULL;
    DefElem *d_new_version = NULL;
    DefElem *d_cascade = NULL;
    char *extschema = NULL;
    char *extversion = NULL;
    bool extcascade = false;
    ListCell *option_cell = NULL;
    char filename[MAXPGPATH];

    foreach (option_cell, options) {
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


    snprintf(filename, MAXPGPATH, "%s/%s/before-create.sql",
             privileged_extensions_custom_scripts_path, extname);
    run_custom_script(filename, extname, extschema, extversion,
                      extcascade);
}

void run_ext_after_create_script(char *extname, List *options, const char *privileged_extensions_custom_scripts_path){
    DefElem *d_schema = NULL;
    DefElem *d_new_version = NULL;
    DefElem *d_cascade = NULL;
    char *extschema = NULL;
    char *extversion = NULL;
    bool extcascade = false;
    ListCell *option_cell = NULL;
    char filename[MAXPGPATH];

    foreach (option_cell, options) {
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

    snprintf(filename, MAXPGPATH, "%s/%s/after-create.sql",
             privileged_extensions_custom_scripts_path, extname);
    run_custom_script(filename, extname, extschema, extversion,
                      extcascade);
}

void override_create_ext_statement(CreateExtensionStmt *stmt, const size_t total_epos, const extension_parameter_overrides *epos){
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
