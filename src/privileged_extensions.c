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

#include "privileged_extensions.h"

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

static void run_process_utility_hook_as_superuser(
    void (*process_utility_hook)(PROCESS_UTILITY_PARAMS),
    PROCESS_UTILITY_PARAMS, char *privileged_extensions_superuser) {
    Oid superuser_oid = BOOTSTRAP_SUPERUSERID;
    Oid prev_role_oid;
    int prev_role_sec_context;

    if (privileged_extensions_superuser != NULL) {
        superuser_oid = get_role_oid(privileged_extensions_superuser, false);
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
    PROCESS_UTILITY_PARAMS, CreateExtensionStmt *stmt,
    char *privileged_extensions, char *privileged_extensions_superuser) {
    if (is_extension_privileged(stmt->extname, privileged_extensions)) {
        run_process_utility_hook_as_superuser(process_utility_hook,
                                              PROCESS_UTILITY_ARGS,
                                              privileged_extensions_superuser);
    } else {
        run_process_utility_hook(process_utility_hook);
    }
}

void handle_alter_extension(
    void (*process_utility_hook)(PROCESS_UTILITY_PARAMS),
    PROCESS_UTILITY_PARAMS, AlterExtensionStmt *stmt,
    char *privileged_extensions, char *privileged_extensions_superuser) {
    if (is_extension_privileged(stmt->extname, privileged_extensions)) {
        run_process_utility_hook_as_superuser(process_utility_hook,
                                              PROCESS_UTILITY_ARGS,
                                              privileged_extensions_superuser);
    } else {
        run_process_utility_hook(process_utility_hook);
    }
}

void handle_drop_extension(void (*process_utility_hook)(PROCESS_UTILITY_PARAMS),
                           PROCESS_UTILITY_PARAMS, DropStmt *stmt,
                           char *privileged_extensions,
                           char *privileged_extensions_superuser) {
    bool all_extensions_are_privileged = true;
    ListCell *lc;

    foreach (lc, stmt->objects) {
        char *name = strVal((Value *)lfirst(lc));

        if (!is_extension_privileged(name, privileged_extensions)) {
            all_extensions_are_privileged = false;
            break;
        }
    }

    if (all_extensions_are_privileged) {
        run_process_utility_hook_as_superuser(process_utility_hook,
                                              PROCESS_UTILITY_ARGS,
                                              privileged_extensions_superuser);
    } else {
        run_process_utility_hook(process_utility_hook);
    }
}
