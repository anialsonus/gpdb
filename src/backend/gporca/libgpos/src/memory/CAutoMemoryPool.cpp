//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2009-2010 Greenplum Inc.
//	Copyright (C) 2011 EMC Corp.
//
//	@filename:
//		CAutoMemoryPool.cpp
//
//	@doc:
//		Implementation for auto memory pool that automatically releases
//  	the attached memory pool and performs leak checking
//
//	@owner:
//
//	@test:
//
//---------------------------------------------------------------------------

#include "gpos/memory/CAutoMemoryPool.h"

#include "gpos/assert.h"
#include "gpos/error/CErrorContext.h"
#include "gpos/error/CErrorHandler.h"
#include "gpos/memory/CMemoryPoolManager.h"
#include "gpos/task/CAutoSuspendAbort.h"
#include "gpos/task/ITask.h"
#include "gpos/types.h"
#include "gpos/utils.h"

using namespace gpos;


//---------------------------------------------------------------------------
//	@function:
//		CAutoMemoryPool::CAutoMemoryPool
//
//	@doc:
//		Create an auto-managed pool; the managed pool is allocated from
//  	the CMemoryPoolManager global instance
//
//---------------------------------------------------------------------------
CAutoMemoryPool::CAutoMemoryPool(ELeakCheck leak_check_type)
	: m_leak_check_type(leak_check_type)
{
	m_mp = CMemoryPoolManager::GetMemoryPoolMgr()->CreateMemoryPool();
}



//---------------------------------------------------------------------------
//	@function:
//		CAutoMemoryPool::Detach
//
//	@doc:
//		Detach function used when CAutoMemoryPool is used to guard a newly
//		created pool until it is safe, e.g., in constructors
//
//---------------------------------------------------------------------------
CMemoryPool *
CAutoMemoryPool::Detach()
{
	CMemoryPool *mp = m_mp;
	m_mp = NULL;

	return mp;
}


//---------------------------------------------------------------------------
//	@function:
//		CAutoMemoryPool::~CAutoMemoryPool
//
//	@doc:
//		Release the pool back to the manager and perform leak checks if
//		(1) strict leak checking indicated, or
//		(2) no checking while pending exception indicated and no pending exception
//
//---------------------------------------------------------------------------
CAutoMemoryPool::~CAutoMemoryPool() noexcept(false)
{
	if (NULL == m_mp)
	{
		return;
	}

	// suspend cancellation
	CAutoSuspendAbort asa;

#ifdef GPOS_DEBUG

	ITask *task = ITask::Self();

	// ElcExc must be used inside tasks only
	GPOS_ASSERT_IMP(ElcExc == m_leak_check_type, NULL != task);

	GPOS_TRY
	{
		if (ElcStrict == m_leak_check_type ||
			(ElcExc == m_leak_check_type && !task->GetErrCtxt()->IsPending()))
		{
			gpos::IOstream &os = gpos::oswcerr;

			// check for leaks, use this to trigger standard Assert handling
			m_mp->AssertEmpty(os);
		}

		// release pool
		CMemoryPoolManager::GetMemoryPoolMgr()->Destroy(m_mp);
	}
	GPOS_CATCH_EX(ex)
	{
		GPOS_ASSERT(
			GPOS_MATCH_EX(ex, CException::ExmaSystem, CException::ExmiAssert));

		// release pool
		CMemoryPoolManager::GetMemoryPoolMgr()->Destroy(m_mp);

		GPOS_RETHROW(ex);
	}
	GPOS_CATCH_END;

#else  // GPOS_DEBUG

	// hand in pool and return
	CMemoryPoolManager::GetMemoryPoolMgr()->Destroy(m_mp);

#endif	// GPOS_DEBUG
}

// EOF
