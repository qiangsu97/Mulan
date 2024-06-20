/* Copyright (c) 2024, Meili Authors */

#ifndef _PACKET_TIMESTAMPING_H
#define _PACKET_TIMESTAMPING_H

#include "../runtime/meili_runtime.h"
#include <stdint.h>
#include <rte_mbuf.h>
#include <rte_mbuf_dyn.h>

/* timestamp is a special type of pipeline stage object 
*  we do not register functions for them and directly use exec function to assign packets with a timestamp using mbuf dyn field 
*  we also bypass other general initialization for these two pipelines and only does specific init 
*/

// #define TS_NAME_START_SUFFIX "TIMESTAMP_START_DNYFIELD"
// #define TS_NAME_END_SUFFIX "TIMESTAMP_END_DNYFIELD"


struct pkt_ts_state{
    char *ts_name;
    int ts_offset;
};



int pkt_ts_exec(int offset, struct rte_mbuf **mbuf, int nb_mbuf);
int pkt_ts_free();
int pkt_ts_init(int *offset);


#endif /* _PACKET_TIMESTAMPING_H */