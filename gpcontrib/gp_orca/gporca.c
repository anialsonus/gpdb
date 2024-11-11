#include "postgres.h"

#include "cdb/cdbvars.h"
#include "miscadmin.h"
#include "utils/builtins.h"
#include "utils/memutils.h"

extern void InitGPOPT();
extern void TerminateGPOPT();

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

	if (Gp_role == GP_ROLE_DISPATCH)
	{
		/* Initialize GPOPT */
		OptimizerMemoryContext = AllocSetContextCreate(TopMemoryContext,
													"GPORCA Top-level Memory Context",
													ALLOCSET_DEFAULT_MINSIZE,
													ALLOCSET_DEFAULT_INITSIZE,
													ALLOCSET_DEFAULT_MAXSIZE);

		InitGPOPT();
	}
}

void
_PG_fini(void)
{
	if (Gp_role == GP_ROLE_DISPATCH)
	{
		TerminateGPOPT();

		if (OptimizerMemoryContext != NULL)
			MemoryContextDelete(OptimizerMemoryContext);
	}
}

