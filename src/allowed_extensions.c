#include <postgres.h>

#include <catalog/namespace.h>
#include <catalog/pg_authid_d.h>
#include <miscadmin.h>
#include <nodes/pg_list.h>
#include <storage/fd.h>
#include <utils/acl.h>
#include <utils/guc.h>
#include <utils/lsyscache.h>
#include <utils/varlena.h>

#include "allowed_extensions.h"

static bool is_extension_allowed(char *name, char *allowed_extensions) {
	bool allowed = false;
    List *allowed_extensions_list;
    ListCell *lc;

    SplitIdentifierString(pstrdup(allowed_extensions), ',', &allowed_extensions_list);

    foreach (lc, allowed_extensions_list) {
        char *allowed_extension = (char *)lfirst(lc);

        if (strcmp(name, allowed_extension) == 0) {
			allowed = true;
			break;
        }
    }
	list_free(allowed_extensions_list);

    return allowed;
}

static void run_process_utility_hook_as_superuser(
    void (*process_utility_hook)(PROCESS_UTILITY_PARAMS),
    PROCESS_UTILITY_PARAMS, char *extensions_superuser) {
    Oid superuser_oid = BOOTSTRAP_SUPERUSERID;
    Oid prev_role_oid;
    int prev_role_sec_context;

    if (extensions_superuser != NULL) {
        superuser_oid = get_role_oid(extensions_superuser, false);
    }

    GetUserIdAndSecContext(&prev_role_oid, &prev_role_sec_context);
    SetUserIdAndSecContext(superuser_oid, prev_role_sec_context |
                                              SECURITY_LOCAL_USERID_CHANGE |
                                              SECURITY_RESTRICTED_OPERATION);

    run_process_utility_hook(process_utility_hook);

    SetUserIdAndSecContext(prev_role_oid, prev_role_sec_context);
}

void handle_create_extension(
    void (*process_utility_hook)(PROCESS_UTILITY_PARAMS),
    PROCESS_UTILITY_PARAMS, CreateExtensionStmt *stmt, char *allowed_extensions,
    char *extensions_superuser) {
    if (is_extension_allowed(stmt->extname, allowed_extensions)) {
        run_process_utility_hook_as_superuser(
            process_utility_hook, PROCESS_UTILITY_ARGS, extensions_superuser);
    } else {
        run_process_utility_hook(process_utility_hook);
    }
}

void handle_alter_extension(
    void (*process_utility_hook)(PROCESS_UTILITY_PARAMS),
    PROCESS_UTILITY_PARAMS, AlterExtensionStmt *stmt, char *allowed_extensions,
    char *extensions_superuser) {
    if (is_extension_allowed(stmt->extname, allowed_extensions)) {
        run_process_utility_hook_as_superuser(
            process_utility_hook, PROCESS_UTILITY_ARGS, extensions_superuser);
    } else {
        run_process_utility_hook(process_utility_hook);
    }
}

void handle_drop_extension(void (*process_utility_hook)(PROCESS_UTILITY_PARAMS),
                           PROCESS_UTILITY_PARAMS, DropStmt *stmt,
                           char *allowed_extensions,
                           char *extensions_superuser) {
    bool all_are_allowed = true;
    ListCell *lc;

    foreach (lc, stmt->objects) {
        char *name = strVal((Value *)lfirst(lc));

        if (!is_extension_allowed(name, allowed_extensions)) {
            all_are_allowed = false;
            break;
        }
    }

    if (all_are_allowed) {
        run_process_utility_hook_as_superuser(
            process_utility_hook, PROCESS_UTILITY_ARGS, extensions_superuser);
    } else {
        run_process_utility_hook(process_utility_hook);
    }
}
