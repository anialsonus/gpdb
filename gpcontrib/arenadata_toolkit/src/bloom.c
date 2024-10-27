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

void
bloom_set(bloom_t * bloom, Oid relnode)
{
	uint32		hashes[MAX_BLOOM_HASH_FUNCS];

	tracking_hashes(relnode, bloom->size, hashes);
	for (int i = 0; i < bloom_hash_num; ++i)
	{
		bloom->map[hashes[i] >> 3] |= 1 << (hashes[i] & 7);
	}
}

void
bloom_init(const uint32 bloom_size, bloom_t *bloom)
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
bloom_copy(bloom_t * src, bloom_t *dest)
{
	dest->size = src->size;
	memcpy(dest->map, src->map, src->size);
	dest->is_set_all = src->is_set_all;
}
