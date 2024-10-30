/*
 * Set of blooms. Main entry point to find a bloom and work with it.
 * Used to track create, extend, truncate events.
 */
#include "bloom_set.h"
#include "tf_shmem.h"

#define BLOOM_ENTRY_GET(set, i) (void *)(set->bloom_entries + i * FULL_BLOOM_ENTRY_SIZE(set->bloom_size));

/*
 * bloom_set api assumes that we are working with the single bloom set.
 * This object is considered as singleton.
 */
bloom_set_t *bloom_set = NULL;

static inline void
bloom_set_check_state(void)
{
	if (tf_shared_state == NULL || bloom_set == NULL)
		ereport(ERROR,
				(errmsg("Failed to access shared memory due to wrong extension initialization"),
				 errhint("Load extension's code through shared_preload_library configuration")));
}

static void
bloom_entry_init(const uint32_t bloom_size, bloom_entry_t * bloom_entry)
{
	bloom_entry->dbid = InvalidOid;
	bloom_init(bloom_size, &bloom_entry->bloom);
}

void
bloom_set_init(const uint32_t bloom_count, const uint32_t bloom_size)
{
	bloom_set = &tf_shared_state->bloom_set;

	bloom_set->bloom_count = bloom_count;
	bloom_set->bloom_size = bloom_size;

	for (uint32_t i = 0; i < bloom_count; i++)
	{
		bloom_entry_t *bloom_entry = BLOOM_ENTRY_GET(bloom_set, i);

		bloom_entry_init(bloom_size, bloom_entry);
	}
}

/*
 * Finds the entry in bloom_set by given dbid.
 * That's a simple linear search, should be reworked (depends on target dbs count).
 */
static bloom_entry_t *
find_bloom_entry(Oid dbid)
{
	bloom_entry_t *bloom_entry;
	int			i = 0;

	for (i = 0; i < bloom_set->bloom_count; i++)
	{
		bloom_entry = BLOOM_ENTRY_GET(bloom_set, i);
		if (bloom_entry->dbid == dbid)
			break;
	}

	if (i == bloom_set->bloom_count)
		return NULL;

	return bloom_entry;
}

/* Bind available filter to given dbid */
bool
bloom_set_bind(Oid dbid)
{
	bloom_entry_t *bloom_entry;

	bloom_set_check_state();

	LWLockAcquire(bloom_set_lock, LW_EXCLUSIVE);
	bloom_entry = find_bloom_entry(dbid);
	if (bloom_entry)
	{
		LWLockRelease(bloom_set_lock);
		return true;
	}
	bloom_entry = find_bloom_entry(InvalidOid);
	if (bloom_entry == NULL)
	{
		LWLockRelease(bloom_set_lock);
		return false;
	}
	bloom_entry->dbid = dbid;
	LWLockBindEntry(dbid);
	LWLockRelease(bloom_set_lock);

	return true;
}

/*
 * Fill the Bloom filter with 0 or 1. Used for setting
 * full snapshots.
 */
bool
bloom_set_trigger_bits(Oid dbid, bool on)
{
	bloom_entry_t *bloom_entry;
	LWLock	   *entry_lock;

	bloom_set_check_state();

	LWLockAcquire(bloom_set_lock, LW_SHARED);
	entry_lock = LWLockAcquireEntry(dbid, LW_EXCLUSIVE);
	bloom_entry = find_bloom_entry(dbid);
	if (bloom_entry)
	{
		if (on)
			bloom_set_all(&bloom_entry->bloom);
		else
			bloom_clear(&bloom_entry->bloom);
		if (entry_lock)
			LWLockRelease(entry_lock);
		LWLockRelease(bloom_set_lock);
		return true;
	}
	if (entry_lock)
		LWLockRelease(entry_lock);
	LWLockRelease(bloom_set_lock);

	if (bloom_entry == NULL)
		elog(LOG, "[arenadata toolkit] tracking_initial_snapshot Bloom filter not found");

	return false;
}

/* Unbind used filter by given dbid */
void
bloom_set_unbind(Oid dbid)
{
	bloom_entry_t *bloom_entry;

	bloom_set_check_state();

	LWLockAcquire(bloom_set_lock, LW_EXCLUSIVE);
	bloom_entry = find_bloom_entry(dbid);
	if (bloom_entry == NULL)
	{
		LWLockRelease(bloom_set_lock);
		return;
	}
	bloom_entry->dbid = InvalidOid;
	bloom_clear(&bloom_entry->bloom);
	LWLockUnbindEntry(dbid);
	LWLockRelease(bloom_set_lock);
}

/* Find bloom by dbid, set bit based on relNode hash */
void
bloom_set_set(Oid dbid, Oid relNode)
{
	bloom_entry_t *bloom_entry;
	LWLock	   *entry_lock;

	bloom_set_check_state();

	LWLockAcquire(bloom_set_lock, LW_SHARED);
	entry_lock = LWLockAcquireEntry(dbid, LW_EXCLUSIVE);
	bloom_entry = find_bloom_entry(dbid);
	if (bloom_entry)
	{
		bloom_set_bits(&bloom_entry->bloom, relNode);
	}
	if (entry_lock)
		LWLockRelease(entry_lock);
	LWLockRelease(bloom_set_lock);
}

/* Find bloom by dbid, copy all bytes to new filter, clear old (but keep it) */
bool
bloom_set_move(Oid dbid, bloom_t * dest)
{
	bloom_entry_t *bloom_entry;
	LWLock	   *entry_lock;

	bloom_set_check_state();

	LWLockAcquire(bloom_set_lock, LW_SHARED);
	entry_lock = LWLockAcquireEntry(dbid, LW_EXCLUSIVE);
	bloom_entry = find_bloom_entry(dbid);
	if (bloom_entry)
	{
		bloom_copy(&bloom_entry->bloom, dest);
		bloom_clear(&bloom_entry->bloom);
		if (entry_lock)
			LWLockRelease(entry_lock);
		LWLockRelease(bloom_set_lock);
		return true;
	}
	if (entry_lock)
		LWLockRelease(entry_lock);
	LWLockRelease(bloom_set_lock);

	return false;
}

/* Find bloom by dbid, merge bytes from another bloom to it */
bool
bloom_set_merge(Oid dbid, bloom_t * from)
{
	bloom_entry_t *bloom_entry;
	LWLock	   *entry_lock;

	bloom_set_check_state();

	if (!from)
		return false;

	LWLockAcquire(bloom_set_lock, LW_SHARED);
	entry_lock = LWLockAcquireEntry(dbid, LW_EXCLUSIVE);
	bloom_entry = find_bloom_entry(dbid);
	if (bloom_entry)
	{
		bloom_merge(&bloom_entry->bloom, from);
		if (entry_lock)
			LWLockRelease(entry_lock);
		LWLockRelease(bloom_set_lock);
		return true;
	}
	if (entry_lock)
		LWLockRelease(entry_lock);
	LWLockRelease(bloom_set_lock);

	return false;
}

bool
bloom_set_is_all_bits_triggered(Oid dbid)
{
	bloom_entry_t *bloom_entry;
	bool		is_triggered = false;
	LWLock	   *entry_lock;

	bloom_set_check_state();

	LWLockAcquire(bloom_set_lock, LW_SHARED);
	entry_lock = LWLockAcquireEntry(dbid, LW_SHARED);
	bloom_entry = find_bloom_entry(dbid);
	if (bloom_entry)
	{
		is_triggered = bloom_entry->bloom.is_set_all;
	}
	if (entry_lock)
		LWLockRelease(entry_lock);
	LWLockRelease(bloom_set_lock);

	return is_triggered;
}
