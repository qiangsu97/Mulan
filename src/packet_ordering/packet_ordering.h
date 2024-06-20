/* Copyright (c) 2024, Meili Authors */

#ifndef _PACKET_ORDERING_H
#define _PACKET_ORDERING_H

#include "../runtime/meili_runtime.h"
#include <stdint.h>
#include <rte_mbuf.h>
#include "../utils/rte_reorder/rte_reorder.h"

/* packet sequencing/reordering are two special type of pipeline stage object 
*  we do not register functions for them and directly use exec function to process packets
*  we also bypass other general initialization for these two pipelines and only does specific init 
*/

#define SEQUENCE_DEFAULT_BATCH_SIZE 64

#define REORDER_BUFFER_SIZE (1 << 16)
#define REORDER_DEFAULT_BATCH_SIZE 64

#define SEQ_NUM_START 0

//#define REORDER_VERIFY_ON

struct seq_state{
    uint32_t seqn; 
};

struct reorder_state{
    struct rte_reorder_buffer* reorder_buf;
    uint32_t nb_staged_buf;
    uint32_t last_seq_nb;
    uint32_t missing_mbuf;
    bool verify;
};


int seq_exec(struct pipeline_stage *self, struct rte_mbuf **mbuf, int nb_mbuf);
int seq_free(struct pipeline_stage *self);
int seq_init(struct pipeline_stage *self);

int reorder_exec(struct pipeline_stage *self, struct rte_mbuf **mbuf, int nb_mbuf, struct rte_mbuf **mbuf_out, int *nb_deq);
int reorder_init(struct pipeline_stage *self);
int reorder_free(struct pipeline_stage *self);

int reorder_verify(struct pipeline_stage *self, struct rte_mbuf **mbuf, int nb_mbuf);

#endif /* _PACKET_ORDERING_H */
