#ifndef BLOOM_SET_H
#define BLOOM_SET_H

#include "postgres.h"

#include "bloom.h"

#define FULL_BLOOM_ENTRY_SIZE(size) (offsetof(bloom_entry_t, bloom) + FULL_BLOOM_SIZE(size))
#define FULL_BLOOM_SET_SIZE(size, count) (offsetof(bloom_set_t, bloom_entries) + FULL_BLOOM_ENTRY_SIZE(size) * count)

/* bloom filter extended by dbid */
typedef struct
{
	Oid			dbid;			/* dbid if binded, InvalidOid if unbinded */
	bloom_t		bloom;
}	bloom_entry_t;

/* static set of all bloom filters */
typedef struct
{
	uint8	bloom_count;	/* count of bloom_entry_t in bloom_entries */
	uint32	bloom_size;		/* size of bloom filter */
	char	bloom_entries[FLEXIBLE_ARRAY_MEMBER];	/* array of
														 * bloom_entry_t */
}	bloom_set_t;

void		bloom_set_init(const uint32 bloom_count, const uint32 bloom_size, bloom_set_t *bloom_set);
bool		bloom_set_bind(bloom_set_t * bloom_set, Oid dbid);
void		bloom_set_unbind(bloom_set_t * bloom_set, Oid dbid);
void		bloom_set_set(bloom_set_t * bloom_set, Oid dbid, Oid relNode);
bool		bloom_set_move(bloom_set_t * bloom_set, Oid dbid, bloom_t *dest);
bool		bloom_set_merge(bloom_set_t * bloom_set, Oid dbid, bloom_t * m_bloom);
bool		bloom_set_trigger_bits(bloom_set_t * bloom_set, Oid dbid, bool on);
bool		bloom_set_is_all_bits_triggered(bloom_set_t * bloom_set, Oid dbid);
int			bloom_set_count(bloom_set_t * bloom_set);

#endif   /* BLOOM_SET_H */
