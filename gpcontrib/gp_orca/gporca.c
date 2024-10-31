#include "postgres.h"

#include "cdb/cdbvars.h"
#include "executor/executor.h"
#include "miscadmin.h"
#include "optimizer/orca.h"
#include "optimizer/planner.h"
#include "storage/shmem.h"
#include "utils/builtins.h"
#include "utils/memutils.h"
#include "utils/guc.h"

extern bool optimizer;

extern void InitGPOPT();
extern void TerminateGPOPT();

extern void compute_jit_flags(PlannedStmt* pstmt);

PG_MODULE_MAGIC;

void		_PG_init(void);
void		_PG_fini(void);

planner_hook_type next_planner_hook = NULL;

static planner_hook_type prev_planner = NULL;

static PlannedStmt *
gp_orca_planner(Query *parse, int cursorOptions, ParamListInfo boundParams)
{
	PlannedStmt	   *result;

	PG_TRY();
	{
		/*
		 * Use ORCA only if it is enabled and we are in a coordinator QD process.
		 *
		 * ORCA excels in complex queries, most of which will access distributed
		 * tables. We can't run such queries from the segments slices anyway because
		 * they require dispatching a query within another - which is not allowed in
		 * GPDB (see querytree_safe_for_qe()). Note that this restriction also
		 * applies to non-QD coordinator slices.  Furthermore, ORCA doesn't currently
		 * support pl/<lang> statements (relevant when they are planned on the segments).
		 * For these reasons, restrict to using ORCA on the coordinator QD processes only.
		 *
		 * PARALLEL RETRIEVE CURSOR is not supported by ORCA yet.
		 */
		if (optimizer &&
			GP_ROLE_DISPATCH == Gp_role &&
			IS_QUERY_DISPATCHER() &&
			(cursorOptions & CURSOR_OPT_SKIP_FOREIGN_PARTITIONS) == 0 &&
			(cursorOptions & CURSOR_OPT_PARALLEL_RETRIEVE) == 0)
		{
			instr_time		starttime;
			instr_time		endtime;

			if (gp_log_optimization_time)
				INSTR_TIME_SET_CURRENT(starttime);

			result = optimize_query(parse, cursorOptions, boundParams);

			/* decide jit state */
			if (result)
			{
				/*
				 * Setting Jit flags for Optimizer
				 */
				compute_jit_flags(result);
			}

			if (gp_log_optimization_time)
			{
				INSTR_TIME_SET_CURRENT(endtime);
				INSTR_TIME_SUBTRACT(endtime, starttime);
				elog(LOG, "Optimizer Time: %.3f ms", INSTR_TIME_GET_MILLISEC(endtime));
			}

			if (result)
				return result;
		}

		if (prev_planner)
			result = (*prev_planner) (parse, cursorOptions, boundParams);
		else
			result = standard_planner(parse, cursorOptions, boundParams);

	}
	PG_CATCH();
	{
		PG_RE_THROW();
	}
	PG_END_TRY();

	return result;
}

void
_PG_init(void)
{
	if (!process_shared_preload_libraries_in_progress)
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

		prev_planner = planner_hook;
		planner_hook = gp_orca_planner;

		/* enable orca here */
		optimizer = true;
	}
}

void
_PG_fini(void)
{
	if (Gp_role == GP_ROLE_DISPATCH)
	{
		/* disable orca here */
		optimizer = false;

		planner_hook = prev_planner;

		TerminateGPOPT();

		if (OptimizerMemoryContext != NULL)
			MemoryContextDelete(OptimizerMemoryContext);
	}
}

