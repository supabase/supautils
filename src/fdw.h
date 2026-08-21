#ifndef FDW_H
#define FDW_H

#include "pg_prelude.h"

extern void verify_fdw_functions_ownership(List *func_options);

#endif
