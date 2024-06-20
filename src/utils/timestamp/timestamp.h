/* Copyright (c) 2024, Meili Authors */

#ifndef _TIMESTAMP_H_
#define _TIMESTAMP_H_

#include <rte_mbuf.h>
#include <rte_mbuf_dyn.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef uint64_t timestamp_t;

/* function prototypes */
int timestamp_init();


/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice
 *
 * Read reorder sequence number from mbuf.
 *
 * @param mbuf Structure to read from.
 * @return pointer to reorder sequence number.
 */
__rte_experimental
static inline timestamp_t *
timestamp(struct rte_mbuf *mbuf, int offset)
{
	return RTE_MBUF_DYNFIELD(mbuf, offset, timestamp_t *);
}

// __rte_experimental
// static inline timestamp_t *
// timestamp_end(struct rte_mbuf *mbuf)
// {
// 	return RTE_MBUF_DYNFIELD(mbuf, timestamp_end_dynfield_offset, timestamp_t *);
// }

#ifdef __cplusplus
}
#endif

#endif /* _TIMESTAMP_H_ */