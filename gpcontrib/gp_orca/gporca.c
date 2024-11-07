#include "postgres.h"

#include "cdb/cdbvars.h"
#include "miscadmin.h"
#include "utils/builtins.h"

PG_MODULE_MAGIC;

void		_PG_init(void);
void		_PG_fini(void);

void
_PG_init(void)
{
	if (!process_shared_preload_libraries_in_progress)
		ereport(ERROR,
				(errcode(ERRCODE_INTERNAL_ERROR),
				 errmsg("This module can only be loaded via shared_preload_libraries")));

	if (!IS_QUERY_DISPATCHER())
		return;
}

void
_PG_fini(void)
{
}

