#include "postgres.h"

#include "cdb/cdbvars.h"
#include "miscadmin.h"
#include "utils/builtins.h"
#include "utils/guc.h"
#include "utils/vmem_tracker.h"

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

	Size orca_mem = 6L << BITS_IN_MB;
	/*
	 * When optimizer_use_gpdb_allocators is on, at least 2MB of above will be
	 * tracked by vmem tracker later, so do not recount them.
	 */
	if (optimizer_use_gpdb_allocators)
		orca_mem -= (2L << BITS_IN_MB);

	GPMemoryProtect_RequestAddinStartupMemory(orca_mem);
}

void
_PG_fini(void)
{
}

