#include "bloom_set.h"
#include "tf_shmem.h"

#include "storage/ipc.h"
#include "storage/shmem.h"

#include "arenadata_toolkit_guc.h"

#include <math.h>

static shmem_startup_hook_type next_shmem_startup_hook = NULL;
tf_shared_state_t *tf_shared_state;
LWLock *tf_state_lock;
LWLock *bloom_set_lock;
tf_entry_lock_t bloom_locks[MAX_DB_TRACK_COUNT];
uint64 bloom_hash_seed;
int bloom_hash_num;

/*
 * Separate initialization of LWLocks;
 */
static void
init_lwlocks(void)
{
	tf_state_lock = LWLockAssign();
	bloom_set_lock = LWLockAssign();

	for (int i = 0; i < db_track_count; ++i)
	{
		bloom_locks[i].lock = LWLockAssign();
		bloom_locks[i].dbid = InvalidOid;
	}
}

/*
 * Calculate the closes power of 2 for given
 * length.
 */
static int
my_bloom_power(uint64 target_bitset_bits)
{
	int			bloom_power = -1;

	while (target_bitset_bits > 0 && bloom_power < 32)
	{
		bloom_power++;
		target_bitset_bits >>= 1;
	}

	return bloom_power;
}

/*
 * Initialize optimal Bloom filter parameters
 *
 * This function calculates and sets optimal parameters for the Bloom filter
 * based on established widespread principles.
 *
 * Adjusts the filter size to the nearest power of 2 to optimize memory alignment
 * and access patterns and simplify modulo operations in hash calculations.
 *
 * Calculates the optimal number of hash functions using the formula:
 * k = (m/n)ln(2), which minimizes the false positive probability
 * p = (1 - e^(-kn/m))^k.
 * where:
 * - m = total_bits (size of bit array)
 * - n = TOTAL_ELEMENTS (expected number of insertions)
 *
 * Initializes bloom_hash_seed with a random value to prevent deterministic hash collisions
 * and ensure independent hash distributions across runs
 *
 * Note: The actual false positive rate might slightly deviate from theoretical
 * optimum due to:
 * - Rounding of k to integer values
 * - Size adjustment to power of 2
 * - Non-perfect independence of double hashing (see bloom.c)
 */
static void
init_bloom_invariants()
{
	uint64 total_bits;
	int k;

	total_bits = UINT64CONST(1) << my_bloom_power(bloom_size * 8);
	bloom_size = total_bits / 8;

	k = rint(log(2.0) * total_bits / TOTAL_ELEMENTS);
	bloom_hash_num = Max(1, Min(k, MAX_BLOOM_HASH_FUNCS));
	bloom_hash_seed = (uint64) random();
}

static Size
tf_shmem_calc_size(void)
{
	Size		size;

	size = offsetof(tf_shared_state_t, bloom_set);
	size = add_size(size, FULL_BLOOM_SET_SIZE(bloom_size, db_track_count));

	return size;
}

static void
tf_shmem_hook(void)
{
	bool		found;
	Size		size;

	init_bloom_invariants();
	size = tf_shmem_calc_size();

	LWLockAcquire(AddinShmemInitLock, LW_EXCLUSIVE);

	tf_shared_state = ShmemInitStruct("toolkit_track_files", size, &found);

	if (!found)
	{
		pg_atomic_init_flag(&tf_shared_state->tracking_is_initialized);
		pg_atomic_init_flag(&tf_shared_state->tracking_error);
		bloom_set_init(db_track_count, bloom_size, &tf_shared_state->bloom_set);
	}

	init_lwlocks();

	LWLockRelease(AddinShmemInitLock);

	if (next_shmem_startup_hook)
		next_shmem_startup_hook();
}

void
tf_shmem_init()
{
	/*
	 * tf_state_lock and bloom_set_lock locks
	 * plus one lock for each db entry.
	 */
	RequestAddinLWLocks(2 + db_track_count);
	RequestAddinShmemSpace(tf_shmem_calc_size());

	next_shmem_startup_hook = shmem_startup_hook;
	shmem_startup_hook = tf_shmem_hook;
}

void
tf_shmem_deinit(void)
{
	shmem_startup_hook = next_shmem_startup_hook;
}

/*
 * Acquire lock corresponding to dbid in bloom_set.
 */
LWLock *
LWLockAcquireEntry(Oid dbid, LWLockMode mode)
{
	LWLockAcquire(tf_state_lock, LW_SHARED);
	for (int i = 0; i < db_track_count; ++i)
	{
		if (bloom_locks[i].dbid == dbid)
		{
			LWLockAcquire(bloom_locks[i].lock, mode);
			LWLockRelease(tf_state_lock);
			return bloom_locks[i].lock;
		}
	}
	LWLockRelease(tf_state_lock);

	return NULL;
}

/*
 * Bind LWLock to tracked dbid.
 */
void
LWLockBindEntry(Oid dbid)
{
	int i;
	LWLockAcquire(tf_state_lock, LW_EXCLUSIVE);
	for (i = 0; i < db_track_count; ++i)
	{
		if (bloom_locks[i].dbid == InvalidOid)
		{
			bloom_locks[i].dbid = dbid;
			break;
		}
	}

	if (i == db_track_count && pg_atomic_unlocked_test_flag(&tf_shared_state->tracking_error))
		pg_atomic_test_set_flag(&tf_shared_state->tracking_error);
	LWLockRelease(tf_state_lock);
}

/*
 * Unbind LWLock from tracked dbid.
 */
void
LWLockUnbindEntry(Oid dbid)
{
	int i;
	LWLockAcquire(tf_state_lock, LW_EXCLUSIVE);
	for (i = 0; i < db_track_count; ++i)
	{
		if (bloom_locks[i].dbid == dbid)
		{
			bloom_locks[i].dbid = InvalidOid;
			break;
		}
	}

	if (i == db_track_count && pg_atomic_unlocked_test_flag(&tf_shared_state->tracking_error))
		pg_atomic_test_set_flag(&tf_shared_state->tracking_error);

	LWLockRelease(tf_state_lock);
}
