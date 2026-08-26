#include "pg_prelude.h"

#include "timezone.h"

/*
 * Mirror of formatting.c internals used to size the to_char() output buffer.
 * These are file-static there and not exported, so we redefine them.
 *
 * The buffer reserves DCH_MAX_ITEM_SIZ bytes per format character, and the
 * TZ/tz format items have a keyword length of 2. The abbreviation must fit in
 * MAX_SAFE_TZ_ABBREV_LEN bytes.
 */
#define DCH_MAX_ITEM_SIZ 12UL
#define DCH_TZ_KEYLEN 2UL
#define MAX_SAFE_TZ_ABBREV_LEN (DCH_TZ_KEYLEN * DCH_MAX_ITEM_SIZ)

/*
 * Postgres's built-in check hook for the timezone GUC.
 */
static GucStringCheckHook prev_check_timezone_hook = NULL;

#if PG18_GTE

/*
 * pg_get_next_timezone_abbrev() function is public starting PG18
 * For previous versions we reimplement it below.
 */

#else

#  include "vendor/pgtz.h"

/*
 * Iteratively fetch all the abbreviations used in the given time zone.
 *
 * *indx is a state counter that the caller must initialize to zero
 * before the first call, and not touch between calls.
 *
 * Returns the next known abbreviation, or NULL if there are no more.
 *
 */
static const char *pg_get_next_timezone_abbrev(int *indx, const pg_tz *tz) {
  const char         *result;
  const struct state *sp = &tz->state;
  const char         *abbrs;
  int                 abbrind;

  /* If we're still in range, the result is the current abbrev. */
  abbrs   = sp->chars;
  abbrind = *indx;
  if (abbrind < 0 || abbrind >= sp->charcnt) return NULL;
  result = abbrs + abbrind;

  /* Advance *indx past this abbrev and its trailing null. */
  while (abbrs[abbrind] != '\0')
    abbrind++;
  abbrind++;
  *indx = abbrind;

  return result;
}

#endif /* PG18_GTE */

/*
 * Returns the longest abbreviation known for the given timezone, or NULL if
 * none is longer than MAX_SAFE_TZ_ABBREV_LEN.
 *
 */
static const char *get_unsafe_abbreviation(pg_tz *tz) {
  int         indx = 0;
  const char *abbr;

  while ((abbr = pg_get_next_timezone_abbrev(&indx, tz)) != NULL) {
    if (strnlen(abbr, MAX_SAFE_TZ_ABBREV_LEN + 1) > MAX_SAFE_TZ_ABBREV_LEN)
      return abbr;
  }

  return NULL;
}

/*
 * Supautil's check hook for the timezone GUC. Rejects timezone
 * values if any of their abbreviations are longer than MAX_SAFE_TZ_ABBREV_LEN
 */
static bool check_timezone(char **newval, void **extra, GucSource source) {

  // Call the built-in check hook before running our checks
  if (prev_check_timezone_hook &&
      !prev_check_timezone_hook(newval, extra, source))
    return false;

  // The built-in hook sets a pg_tz instance in the `extra` variable. If
  // there was no built-in hook to run (or it ran but didn't set `extra`),
  // there's no pg_tz to check here, so don't dereference NULL.
  if (*extra == NULL) return true;

  pg_tz      *tz   = *((pg_tz **)*extra);
  const char *abbr = get_unsafe_abbreviation(tz);

  if (abbr) {
    GUC_check_errmsg("time zone abbreviation \"%s\" is too long", abbr);
    return false;
  }

  return true;
}

/*
 * Finds the timezone GUC config struct. It returns a generic
 * config struct which the callers need to check and cast to
 * appropriate types.
 */
static struct config_generic *find_timezone_guc(void) {
#if PG16_GTE
  return find_option("timezone", false, true, ERROR);
#else
  struct config_generic **guc_vars = get_guc_variables();
  int                     num_vars = GetNumConfigOptions();

  for (int i = 0; i < num_vars; i++)
    if (pg_strcasecmp(guc_vars[i]->name, "timezone") == 0) return guc_vars[i];

  return NULL;
#endif
}

/*
 * Override the timezone GUC's built-in check hook with our own function
 * to allow us to intercept the timezone values with abbreviations
 * longer than MAX_SAFE_TZ_ABBREV_LEN. This takes control away from the
 * attacker to exploit CVE-2026-14669 even on unpatched Postgres versions.
 */
void hook_timezone_check(void) {
  struct config_generic *guc = find_timezone_guc();

  if (guc == NULL)
    ereport(ERROR, (errmsg("supautils: could not find the \"TimeZone\" GUC")));

  if (guc->vartype != PGC_STRING)
    ereport(ERROR,
            (errmsg("supautils: \"TimeZone\" GUC is not a string variable")));

  struct config_string *timezone_guc = (struct config_string *)guc;

  prev_check_timezone_hook = timezone_guc->check_hook;
  timezone_guc->check_hook = check_timezone;
}

/*
 * At startup supautils is loaded after GUC variables in the startup packet
 * are already applied. This means check_timezone won't be called as it
 * will be hooked too late. This function checks the existing timezone
 * value set via the startup packet and rejects it if its abbreviation is
 * too long.
 */
void check_timezone_at_startup(void) {
  if (!session_timezone) return;

  const char *abbr = get_unsafe_abbreviation(session_timezone);

  if (abbr)
    ereport(FATAL, (errmsg("time zone abbreviation \"%s\" is too long", abbr)));
}
