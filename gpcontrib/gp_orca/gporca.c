#include "postgres.h"

#include "cdb/cdbvars.h"
#include "commands/explain.h"
#include "miscadmin.h"
#include "utils/builtins.h"
#include "utils/guc.h"
#include "utils/vmem_tracker.h"

#include "nodes/extensible.h"
#include "nodes/makefuncs.h"
#include "nodes/nodeFuncs.h"

#include "optimizer/clauses.h"
#include "optimizer/orca.h"

static ExplainOneQuery_hook_type prev_explain = NULL;

extern char *SerializeDXLPlan(Query *parse);

PG_MODULE_MAGIC;

void		_PG_init(void);
void		_PG_fini(void);


/*
 * ExplainDXL -
 *	  print out the execution plan for one Query in DXL format
 *	  this function implicitly uses optimizer
 */
static void
ExplainDXL(Query *query, ExplainState *es, const char *queryString,
				ParamListInfo params)
{
	MemoryContext oldcxt = CurrentMemoryContext;
	bool		save_enumerate;
	char	   *dxl = NULL;
	PlannerInfo		*root;
	PlannerGlobal	*glob;
	Query			*pqueryCopy;

	save_enumerate = optimizer_enumerate_plans;

	/* Do the EXPLAIN. */

	/* enable plan enumeration before calling optimizer */
	optimizer_enumerate_plans = true;

	/*
	 * Initialize a dummy PlannerGlobal struct. ORCA doesn't use it, but the
	 * pre- and post-processing steps do.
	 */
	glob = makeNode(PlannerGlobal);
	glob->subplans = NIL;
	glob->subroots = NIL;
	glob->rewindPlanIDs = NULL;
	glob->transientPlan = false;
	glob->oneoffPlan = false;
	glob->share.shared_inputs = NULL;
	glob->share.shared_input_count = 0;
	glob->share.motStack = NIL;
	glob->share.qdShares = NULL;
	/* these will be filled in below, in the pre- and post-processing steps */
	glob->finalrtable = NIL;
	glob->relationOids = NIL;
	glob->invalItems = NIL;

	root = makeNode(PlannerInfo);
	root->parse = query;
	root->glob = glob;
	root->query_level = 1;
	root->planner_cxt = CurrentMemoryContext;
	root->wt_param_id = -1;

	/* create a local copy to hand to the optimizer */
	pqueryCopy = (Query *) copyObject(query);

	/*
	 * Pre-process the Query tree before calling optimizer.
	 *
	 * Constant folding will add dependencies to functions or relations in
	 * glob->invalItems, for any functions that are inlined or eliminated
	 * away. (We will find dependencies to other objects later, after planning).
	 */
	pqueryCopy = fold_constants(root, pqueryCopy, params, GPOPT_MAX_FOLDED_CONSTANT_SIZE);

	/*
	 * If any Query in the tree mixes window functions and aggregates, we need to
	 * transform it such that the grouped query appears as a subquery
	 */
	pqueryCopy = (Query *) transformGroupedWindows((Node *) pqueryCopy, NULL);


	/* optimize query using optimizer and get generated plan in DXL format */
	dxl = SerializeDXLPlan(pqueryCopy);

	/* restore old value of enumerate plans GUC */
	optimizer_enumerate_plans = save_enumerate;

	if (dxl == NULL)
		elog(NOTICE, "Optimizer failed to produce plan");
	else
	{
		appendStringInfoString(es->str, dxl);
		appendStringInfoChar(es->str, '\n'); /* separator line */
		pfree(dxl);
	}

	/* Free the memory we used. */
	MemoryContextSwitchTo(oldcxt);
}

static void gp_orca_explain (Query *query,
					int cursorOptions,
					IntoClause *into,
					ExplainState *es,
					const char *queryString,
					ParamListInfo params,
					QueryEnvironment *queryEnv)
{
	if (es->dxl)
	{
		ExplainDXL(query, es, queryString, params);
		return;
	}

	if (prev_explain)
		(*prev_explain)(query, cursorOptions, into, es,
							 queryString, params, queryEnv);
	else
		standard_ExplainOneQuery(query, cursorOptions, into, es,
							 queryString, params, queryEnv);

}


void
_PG_init(void)
{
	if (!process_shared_preload_libraries_in_progress)
		ereport(ERROR,
				(errcode(ERRCODE_INTERNAL_ERROR),
				 errmsg("This module can only be loaded via shared_preload_libraries")));

	if (!IS_QUERY_DISPATCHER())
		return;

	/* When compile with ORCA it will commit 6MB more */
	Size orca_mem = 6L << BITS_IN_MB;
	/*
	 * When optimizer_use_gpdb_allocators is on, at least 2MB of above will be
	 * tracked by vmem tracker later, so do not recount them.
	 */
	if (optimizer_use_gpdb_allocators)
		orca_mem -= (2L << BITS_IN_MB);

	GPMemoryProtect_RequestAddinStartupMemory(orca_mem);

	prev_explain = ExplainOneQuery_hook;
	ExplainOneQuery_hook = gp_orca_explain;
}

void
_PG_fini(void)
{
	ExplainOneQuery_hook = prev_explain;
}

