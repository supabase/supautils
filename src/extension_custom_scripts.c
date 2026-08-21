#include "extension_custom_scripts.h"
#include <errno.h>
#include <sys/stat.h>

// Prevent recursively running custom scripts
static bool running_custom_script = false;

// This produces a quoted SQL string literal, e.g. x -> 'x'. NULL is
// substituted as the bare SQL NULL keyword, so an absent CREATE EXTENSION
// option (schema, version) reads as an actual null once substituted into a
// script, not the string "null".
static char *sql_literal(const char *str) {
  return str == NULL ? "null" : quote_literal_cstr(str);
}

/*
 * Read the whole of file into memory.
 *
 * The file contents are returned as a single palloc'd chunk. For convenience
 * of the callers, an extra \0 byte is added to the end. Returns NULL if the
 * file does not exist, since most extensions don't have a custom script.
 */
static char *read_whole_file(const char *filename, int *length) {
  struct stat fst;

  if (stat(filename, &fst) < 0) {
    if (errno == ENOENT) return NULL;

    ereport(ERROR, (errcode_for_file_access(),
                    errmsg("could not stat file \"%s\": %m", filename)));
  }

  size_t bytes_to_read = (size_t)fst.st_size;

  if (bytes_to_read > (MaxAllocSize - 1))
    ereport(ERROR, (errcode(ERRCODE_PROGRAM_LIMIT_EXCEEDED),
                    errmsg("file \"%s\" is too large", filename)));

  FILE *file;
  if ((file = AllocateFile(filename, PG_BINARY_R)) == NULL)
    ereport(ERROR,
            (errcode_for_file_access(),
             errmsg("could not open file \"%s\" for reading: %m", filename)));

  char *buf = (char *)palloc(bytes_to_read + 1);

  *length = fread(buf, 1, bytes_to_read, file);

  if (ferror(file))
    ereport(ERROR, (errcode_for_file_access(),
                    errmsg("could not read file \"%s\": %m", filename)));

  FreeFile(file);

  buf[*length] = '\0';
  return buf;
}

/*
 * Read an SQL script file into a string, and convert to database encoding.
 * Returns NULL if the file does not exist.
 */
static char *read_custom_script_file(const char *filename) {
  int len;

  char *src_str = read_whole_file(filename, &len);

  if (src_str == NULL) return NULL;

  int src_encoding = GetDatabaseEncoding();

  /* make sure that source string is valid in the expected encoding */
  (void)pg_verify_mbstr(src_encoding, src_str, len, false);

  /*
   * Convert the encoding to the database encoding. read_whole_file
   * null-terminate the string, so if no conversion happens the string is
   * valid as is.
   */
  char *dest_str = pg_any_to_server(src_str, len, src_encoding);

  return dest_str;
}

/*
 * Substitute @extname@, @extschema@, @extversion@, and @extcascade@ in the
 * SQL script with their corresponding values, quoted as SQL string literals.
 *
 * This must do a single left-to-right pass over c_sql and never rescan text
 * that was just substituted in. Otherwise a malicious extension name/schema/
 * version containing one of these placeholders (e.g. an extname of literally
 * "@extschema@") would get substituted a second time, letting an attacker
 * inject arbitrary SQL into the script. See
 * extension_custom_scripts_privilege_escalation.sql for the exploit this
 * guards against.
 */
static char *substitute_variables(const char *sql, const char *extname,
                                  const char *extschema, const char *extversion,
                                  bool extcascade) {
  /*
   * These values get substituted as single-quoted string literals. When the
   * arg contains one of the following characters, no one collection of
   * quoting can work inside $$dollar-quoted string literals$$,
   * 'single-quoted string literals', and outside of any literal. To
   * avoid a security snare for custom script authors, error on substitution
   * for arguments containing these.
   */
  const char *quoting_relevant_chars = "\"$'\\";

  if (extname && strpbrk(extname, quoting_relevant_chars))
    ereport(ERROR, (errcode(ERRCODE_INVALID_TEXT_REPRESENTATION),
                    errmsg("invalid character in extension name: must not "
                           "contain any of \"%s\"",
                           quoting_relevant_chars)));

  if (extschema && strpbrk(extschema, quoting_relevant_chars))
    ereport(ERROR, (errcode(ERRCODE_INVALID_TEXT_REPRESENTATION),
                    errmsg("invalid character in extension schema: must not "
                           "contain any of \"%s\"",
                           quoting_relevant_chars)));

  if (extversion && strpbrk(extversion, quoting_relevant_chars))
    ereport(ERROR, (errcode(ERRCODE_INVALID_TEXT_REPRESENTATION),
                    errmsg("invalid character in extension version: must not "
                           "contain any of \"%s\"",
                           quoting_relevant_chars)));

  // We don't check extcascade because it's a bool and user can't pass
  // an arbitrary string in it

  const char *quoted_extname    = sql_literal(extname);
  const char *quoted_extschema  = sql_literal(extschema);
  const char *quoted_extversion = sql_literal(extversion);
  const char *quoted_extcascade = extcascade ? "true" : "false";

  StringInfoData out;
  initStringInfo(&out);

  for (const char *p = sql; *p != '\0';) {
    if (strncmp(p, "@extname@", 9) == 0) {
      appendStringInfoString(&out, quoted_extname);
      p += 9;
    } else if (strncmp(p, "@extschema@", 11) == 0) {
      appendStringInfoString(&out, quoted_extschema);
      p += 11;
    } else if (strncmp(p, "@extversion@", 12) == 0) {
      appendStringInfoString(&out, quoted_extversion);
      p += 12;
    } else if (strncmp(p, "@extcascade@", 12) == 0) {
      appendStringInfoString(&out, quoted_extcascade);
      p += 12;
    } else {
      appendStringInfoChar(&out, *p);
      p += 1;
    }
  }

  return out.data;
}

static void run_custom_script(const char *filename, const char *extname,
                              const char *extschema, const char *extversion,
                              bool extcascade) {
  if (running_custom_script) {
    return;
  }

  char *sql = read_custom_script_file(filename);
  if (sql == NULL) {
    return; // no custom script for this extension/event
  }

  running_custom_script = true;

  PG_TRY();
  {
    sql = substitute_variables(sql, extname, extschema, extversion, extcascade);

    PushActiveSnapshot(GetTransactionSnapshot());

    int rc = SPI_connect();
    if (rc != SPI_OK_CONNECT) {
      elog(ERROR, "SPI_connect failed with error code %d", rc);
    }

    rc = SPI_execute(sql, false, 0);
    if (rc != SPI_OK_UTILITY) {
      elog(ERROR, "SPI_execute failed with error code %d", rc);
    }

    rc = SPI_finish();
    if (rc != SPI_OK_FINISH) {
      elog(ERROR, "SPI_finish failed with error code %d", rc);
    }

    PopActiveSnapshot();
  }
  PG_CATCH();
  {
    running_custom_script = false;
    PG_RE_THROW();
  }
  PG_END_TRY();

  running_custom_script = false;
}

void run_global_before_create_script(
    char *extname, List *options,
    const char *privileged_extensions_custom_scripts_path) {
  DefElem *d_schema = NULL, *d_new_version = NULL, *d_cascade = NULL;
  char    *extschema = NULL, *extversion = NULL;
  bool     extcascade = false;
  char     filename[MAXPGPATH];

  ListCell *option_cell = NULL;

  foreach (option_cell, options) {
    DefElem *defel = lfirst_node(DefElem, option_cell);

    if (strcmp(defel->defname, "schema") == 0) {
      d_schema  = defel;
      extschema = defGetString(d_schema);
    } else if (strcmp(defel->defname, "new_version") == 0) {
      d_new_version = defel;
      extversion    = defGetString(d_new_version);
    } else if (strcmp(defel->defname, "cascade") == 0) {
      d_cascade  = defel;
      extcascade = defGetBoolean(d_cascade);
    }
  }

  snprintf(filename, MAXPGPATH, "%s/before-create.sql",
           privileged_extensions_custom_scripts_path);
  run_custom_script(filename, extname, extschema, extversion, extcascade);
}

void run_ext_before_create_script(
    char *extname, List *options,
    const char *privileged_extensions_custom_scripts_path) {
  DefElem  *d_schema      = NULL;
  DefElem  *d_new_version = NULL;
  DefElem  *d_cascade     = NULL;
  char     *extschema     = NULL;
  char     *extversion    = NULL;
  bool      extcascade    = false;
  ListCell *option_cell   = NULL;
  char      filename[MAXPGPATH];

  foreach (option_cell, options) {
    DefElem *defel = lfirst_node(DefElem, option_cell);

    if (strcmp(defel->defname, "schema") == 0) {
      d_schema  = defel;
      extschema = defGetString(d_schema);
    } else if (strcmp(defel->defname, "new_version") == 0) {
      d_new_version = defel;
      extversion    = defGetString(d_new_version);
    } else if (strcmp(defel->defname, "cascade") == 0) {
      d_cascade  = defel;
      extcascade = defGetBoolean(d_cascade);
    }
  }

  snprintf(filename, MAXPGPATH, "%s/%s/before-create.sql",
           privileged_extensions_custom_scripts_path, extname);
  run_custom_script(filename, extname, extschema, extversion, extcascade);
}

void run_ext_after_create_script(
    char *extname, List *options,
    const char *privileged_extensions_custom_scripts_path) {
  DefElem  *d_schema      = NULL;
  DefElem  *d_new_version = NULL;
  DefElem  *d_cascade     = NULL;
  char     *extschema     = NULL;
  char     *extversion    = NULL;
  bool      extcascade    = false;
  ListCell *option_cell   = NULL;
  char      filename[MAXPGPATH];

  foreach (option_cell, options) {
    DefElem *defel = lfirst_node(DefElem, option_cell);

    if (strcmp(defel->defname, "schema") == 0) {
      d_schema  = defel;
      extschema = defGetString(d_schema);
    } else if (strcmp(defel->defname, "new_version") == 0) {
      d_new_version = defel;
      extversion    = defGetString(d_new_version);
    } else if (strcmp(defel->defname, "cascade") == 0) {
      d_cascade  = defel;
      extcascade = defGetBoolean(d_cascade);
    }
  }

  snprintf(filename, MAXPGPATH, "%s/%s/after-create.sql",
           privileged_extensions_custom_scripts_path, extname);
  run_custom_script(filename, extname, extschema, extversion, extcascade);
}
