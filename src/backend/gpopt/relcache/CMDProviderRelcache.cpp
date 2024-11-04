//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 EMC Corp.
//
//	@filename:
//		CMDProviderRelcache.cpp
//
//	@doc:
//		Implementation of a relcache-based metadata provider, which uses GPDB's
//		relcache to lookup objects given their ids.
//
//	@test:
//
//
//---------------------------------------------------------------------------
#include "gpopt/utils/gpdbdefs.h"
#include "gpopt/mdcache/CMDAccessor.h"
#include "gpopt/relcache/CMDProviderRelcache.h"
#include "gpopt/translate/CTranslatorRelcacheToDXL.h"
#include "naucrates/dxl/CDXLUtils.h"
#include "naucrates/exception.h"

using namespace gpos;
using namespace gpdxl;
using namespace gpmd;

CWStringBase *
CMDProviderRelcache::GetMDObjDXLStr(CMemoryPool *mp __attribute__ ((unused)),
									CMDAccessor *md_accessor __attribute__ ((unused)),
									IMDId *md_id __attribute__ ((unused))) const
{
	// not used
	return nullptr;
}

// return the requested metadata object
IMDCacheObject *
CMDProviderRelcache::GetMDObj(CMemoryPool *mp, CMDAccessor *md_accessor,
							  IMDId *mdid, IMDCacheObject::Emdtype mdtype) const
{
	IMDCacheObject *md_obj =
		CTranslatorRelcacheToDXL::RetrieveObject(mp, md_accessor, mdid, mdtype);
	GPOS_ASSERT(nullptr != md_obj);

	return md_obj;
}

// EOF
