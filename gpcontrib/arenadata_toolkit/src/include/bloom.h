#ifndef BLOOM_H
#define BLOOM_H

#include "postgres.h"

#include <stdint.h>

#define MAX_BLOOM_HASH_FUNCS 6
#define TOTAL_ELEMENTS 100000000UL
#define FULL_BLOOM_SIZE(size) (offsetof(bloom_t, map) + size)

typedef struct
{
	uint32_t	size;			/* size in bytes of 'map' */
	int			is_set_all;		/* is all bits sets by bloom_set_all */
	char		map[FLEXIBLE_ARRAY_MEMBER]; /* filter itself, array of bytes */ ;
}	bloom_t;

void		bloom_init(const uint32 bloom_size, bloom_t* bloom);
bool		bloom_isset(bloom_t * bloom, Oid relnode);
void		bloom_set(bloom_t * bloom, Oid relnode);
void		bloom_set_all(bloom_t * bloom);
void		bloom_clear(bloom_t * bloom);
void		bloom_merge(bloom_t * dst, bloom_t * src);
void		bloom_copy(bloom_t * src, bloom_t *dest);

#endif   /* BLOOM_H */
