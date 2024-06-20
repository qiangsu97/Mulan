/* Copyright (c) 2024, Meili Authors */

/*
 * Packet timestamping
 * - Register a timestamping field using mbuf dynfield
 */

#include "packet_timestamping.h"
#include "../utils/timestamp/timestamp.h"


#include <stdlib.h>
//#include <pthread.h>
#include <math.h>
#include <limits.h>
#include <string.h>

#include <rte_eal.h>
#include <rte_common.h>
#include <rte_errno.h>
#include <rte_ethdev.h>
#include <rte_lcore.h>
#include <rte_malloc.h>
#include <rte_mbuf.h>
#include <rte_mempool.h>
#include <rte_ring.h>
// #include "rte_reorder/rte_reorder.h"

#include "../lib/log/meili_log.h"

int
pkt_ts_init(int *offset)
{
    /* allocate space for pipeline state */
    // self->state = (struct pkt_ts_state *)malloc(sizeof(struct pkt_ts_state));
    // struct pkt_ts_state *mystate = (struct pkt_ts_state *)self->state;
    // if(!mystate){
    //     return -ENOMEM;
    // }

    /* register timestamp */
    *offset = timestamp_init();

	if (*offset < 0) {
		MEILI_LOG_ERR("Failed to register mbuf field for tiemstamp, rte_errno: %i", rte_errno);
		return -ENOMEM;
	}
    //memset(self->state, 0x00, sizeof(struct entropy_state));

    return 0;
}

int
pkt_ts_free()
{
    // struct pkt_ts_state *mystate = (struct pkt_ts_state *)self->state;
    // if(mystate){
    //     free(mystate);
    // }
    return 0;
}

int
pkt_ts_exec(int offset, struct rte_mbuf **mbuf, int nb_mbuf)
{
    /* RTE_MBUF_DYNFIELD(mbuf, timestamp_dynfield_offset, uint64_t *); to access the field and get its value*/
    // TODO: confirm if store 32 as 64 function in rxpbench is faster than normal access using macros provided by library

    uint64_t *ts = NULL;
    uint64_t time;
    

    time = rte_get_timer_cycles();

    /* assign timestamp for the packets */
    for(int i=0; i<nb_mbuf; i++){
        ts = RTE_MBUF_DYNFIELD(mbuf[i], offset, uint64_t *);
        *ts = time;
    }


    return 0;
}

