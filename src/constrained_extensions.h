#ifndef CONSTRAINED_EXTENSIONS_H
#define CONSTRAINED_EXTENSIONS_H

#include <postgres.h>

typedef struct
{
	char *name;
	int cpu;
	uint64 mem;
	uint64 disk;
} constrained_extension;

typedef enum
{
	JCE_EXPECT_TOPLEVEL_START,
	JCE_EXPECT_TOPLEVEL_FIELD,
	JCE_EXPECT_CONSTRAINTS_START,
	JCE_EXPECT_CPU,
	JCE_EXPECT_MEM,
	JCE_EXPECT_DISK,
	JCE_UNEXPECTED_FIELD,
	JCE_UNEXPECTED_ARRAY,
	JCE_UNEXPECTED_SCALAR,
	JCE_UNEXPECTED_OBJECT,
	JCE_UNEXPECTED_CPU_VALUE,
	JCE_UNEXPECTED_MEM_VALUE,
	JCE_UNEXPECTED_DISK_VALUE
} json_constrained_extension_semantic_state;

typedef struct
{
	json_constrained_extension_semantic_state state;
	char* error_msg;
	int total_cexts;
	constrained_extension *cexts;
} json_constrained_extension_parse_state;

extern json_constrained_extension_parse_state
parse_constrained_extensions(
	const char *str,
	constrained_extension *cexts
);

void constrain_extension(
	const char* name,
	constrained_extension *cexts,
	const size_t total_cexts);

#endif
