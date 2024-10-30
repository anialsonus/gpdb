#ifndef BLOOM_SET_H
#define BLOOM_SET_H

#include "postgres.h"

#include "bloom.h"

#define FULL_BLOOM_ENTRY_SIZE(size) (offsetof(bloom_entry_t, bloom) + FULL_BLOOM_SIZE(size))
#define FULL_BLOOM_SET_SIZE(size, count) (offsetof(bloom_set_t, bloom_entries) + FULL_BLOOM_ENTRY_SIZE(size) * count)

/* Bloom set entry. */
typedef struct
{
	Oid			dbid;			/* dbid of tracked database or InvalidOid */
	bloom_t		bloom;			/* bloom filter itself */
}	bloom_entry_t;

/* Set of all allocated bloom filters*/
typedef struct
{
	uint8		bloom_count;	/* count of bloom_entry_t in bloom_entries */
	uint32		bloom_size;		/* size of bloom filter */
	char		bloom_entries[FLEXIBLE_ARRAY_MEMBER];	/* array of
														 * bloom_entry_t */
}	bloom_set_t;

void		bloom_set_init(const uint32 bloom_count, const uint32 bloom_size);
bool		bloom_set_bind(Oid dbid);
void		bloom_set_unbind(Oid dbid);
void		bloom_set_set(Oid dbid, Oid relNode);
bool		bloom_set_move(Oid dbid, bloom_t * dest);
bool		bloom_set_merge(Oid dbid, bloom_t * from);
bool		bloom_set_trigger_bits(Oid dbid, bool on);
bool		bloom_set_is_all_bits_triggered(Oid dbid);

#endif   /* BLOOM_SET_H */
