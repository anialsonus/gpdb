/*
 * Simple bloom filter without using postgres primitives.
 */
#include "bloom.h"
#include "hashimpl.h"
#include "tf_shmem.h"

static inline uint32
mod_m(uint32 val, uint64 m)
{
	Assert(m <= PG_UINT32_MAX + UINT64CONST(1));
	Assert(((m - 1) & m) == 0);

	return val & (m - 1);
}

/*
 * Generate k independent bit positions in a Bloom filter.
 *
 * Implements Enhanced Double Hashing technique (Dillinger & Manolios, 2004) which
 * generates k hash values using only 2 independent hash functions. This approach
 * provides comparable performance to using k independent hash functions while
 * being more computationally efficient.
 *
 * Algorithm:
 * 1. Generate two independent 32-bit hashes (x, y) from a 64-bit wyhash
 * 2. Apply modulo operation to fit within filter size
 * 3. Generate subsequent indices using linear combination: x = (x + y) mod m
 *														  y = (y + i) mod m
 *
 * Parameters:
 * node		   - relation file node OID to hash
 * bloom_size  - size of Bloom filter in bytes
 * out_hashes  - output array to store k bit positions
 *
 * Reference: GPDB7 codebase.
 */
static void
tracking_hashes(Oid node, uint32 bloom_size, uint32 *out_hashes)
{
	uint64		hash;
	uint32		x,
				y;
	uint64		m;
	int			i;

	/* Use 64-bit hashing to get two independent 32-bit hashes */
	hash = wyhash(node, bloom_hash_seed);
	x = (uint32) hash;
	y = (uint32) (hash >> 32);
	m = bloom_size * 8;

	x = mod_m(x, m);
	y = mod_m(y, m);

	/* Accumulate hashes */
	out_hashes[0] = x;
	for (i = 1; i < bloom_hash_num; i++)
	{
		x = mod_m(x + y, m);
		y = mod_m(y + i, m);

		out_hashes[i] = x;
	}
}

/*
* Test membership of an element in Bloom filter
*
* Implements standard Bloom filter membership test by checking k different bit
* positions. The function provides probabilistic set membership with controllable
* false positive rate.
*
* Returns true if element might be in set, false if definitely not in set.
*/
bool
bloom_isset(bloom_t * bloom, Oid relnode)
{
	uint32		hashes[MAX_BLOOM_HASH_FUNCS];

	if (bloom->is_set_all)
		return true;

	tracking_hashes(relnode, bloom->size, hashes);

	for (int i = 0; i < bloom_hash_num; ++i)
	{
		if (!(bloom->map[hashes[i] >> 3] & (1 << (hashes[i] & 7))))
			return false;
	}
	return true;
}

/*
 * Insert an element into Bloom filter
 *
 * Sets k bits in the Bloom filter's bit array corresponding to the k hash
 * values generated for the input element. This operation is irreversible -
 * elements cannot be removed without rebuilding the entire filter.
 *
 * Parameters:
 * bloom	- pointer to Bloom filter structure
 * relnode	- relation file node OID to insert
 */
void
bloom_set_bits(bloom_t * bloom, Oid relnode)
{
	uint32		hashes[MAX_BLOOM_HASH_FUNCS];

	tracking_hashes(relnode, bloom->size, hashes);
	for (int i = 0; i < bloom_hash_num; ++i)
	{
		bloom->map[hashes[i] >> 3] |= 1 << (hashes[i] & 7);
	}
}

void
bloom_init(const uint32 bloom_size, bloom_t * bloom)
{
	bloom->size = bloom_size;
	bloom_clear(bloom);
}

void
bloom_set_all(bloom_t * bloom)
{
	memset(bloom->map, 0xFF, bloom->size);
	bloom->is_set_all = 1;
}

void
bloom_clear(bloom_t * bloom)
{
	memset(bloom->map, 0, bloom->size);
	bloom->is_set_all = 0;
}

void
bloom_merge(bloom_t * dst, bloom_t * src)
{
	for (uint32_t i = 0; i < dst->size; i++)
		dst->map[i] |= src->map[i];
	if (src->is_set_all)
		dst->is_set_all = src->is_set_all;
}

void
bloom_copy(bloom_t * src, bloom_t * dest)
{
	dest->size = src->size;
	memcpy(dest->map, src->map, src->size);
	dest->is_set_all = src->is_set_all;
}
