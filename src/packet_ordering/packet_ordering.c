/* Copyright (c) 2024, Meili Authors */

/*
 * Packet ordering
 * - Split flows based on five-tuple
 * - Assign unique seq number for packets 
 * - Reorder packets based on seq num before leaving pipleine
 */

#include "packet_ordering.h"


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
#include "../utils/rte_reorder/rte_reorder.h"

#include "../lib/log/meili_log.h"


int
seq_init(struct pipeline_stage *self)
{
    /* allocate space for pipeline state */
    self->state = (struct seq_state *)malloc(sizeof(struct seq_state));
    struct seq_state *mystate = (struct seq_state *)self->state;
    if(!mystate){
        return -ENOMEM;
    }

    self->batch_size = SEQUENCE_DEFAULT_BATCH_SIZE;
    //memset(self->state, 0x00, sizeof(struct entropy_state));

    mystate->seqn = SEQ_NUM_START;

    return 0;
}

int
seq_free(struct pipeline_stage *self)
{
    struct seq_state *mystate = (struct seq_state *)self->state;
    if(mystate){
        free(mystate);
    }
    return 0;
}

int
seq_exec(struct pipeline_stage *self, struct rte_mbuf **mbuf, int nb_mbuf)
{
    /* rte_reorder_seqn_t a.k.a uint32_t */
    struct seq_state *mystate = (struct seq_state *)self->state;
    uint32_t *seqn = &(mystate->seqn);

    for(int i=0; i<nb_mbuf; i++){
        /* assign seq num for the packet */
        *rte_reorder_seqn(mbuf[i]) = *seqn;
        
        *seqn = *seqn + 1;
    }


    return 0;
}


int
reorder_init(struct pipeline_stage *self)
{
    /* allocate space for pipeline state */
    self->state = (struct reorder_state *)malloc(sizeof(struct reorder_state));
    struct reorder_state *mystate = (struct reorder_state *)self->state;
    if(!mystate){
        return -ENOMEM;
    }
    
    self->batch_size = REORDER_DEFAULT_BATCH_SIZE;
    mystate->reorder_buf = rte_reorder_create("PKT_RO", rte_socket_id(),
				REORDER_BUFFER_SIZE);
    
    rte_reorder_min_seqn_set(mystate->reorder_buf, SEQ_NUM_START);

    mystate->last_seq_nb = SEQ_NUM_START-1;
    mystate->nb_staged_buf = 0;
    // mystate->verify = true;

    return 0;
}

int
reorder_free(struct pipeline_stage *self)
{
    struct reorder_state *mystate = (struct reorder_state *)self->state;
    //free(mystate->reorder_buf);
    free(mystate);
    
    return 0;
}

int
reorder_exec(struct pipeline_stage *self, struct rte_mbuf **mbuf, int nb_mbuf, struct rte_mbuf **mbuf_drain, int *nb_deq)
{
    int ret;
    int dret;
    int batch_size = self->batch_size;

    int seq_num;

    struct reorder_state *mystate = (struct reorder_state *)self->state;


    for(int i=0 ; i<nb_mbuf; i++){
        seq_num = *rte_reorder_seqn(mbuf[i]);
        ret = rte_reorder_insert(mystate->reorder_buf, mbuf[i]);
        if (unlikely(ret == -1 && rte_errno == ERANGE)) {
            /* Too early pkts should be transmitted out directly */

            printf("%s():Cannot reorder early packet, seq=%d\n", __func__, *rte_reorder_seqn(mbuf[i]));
        } else if (unlikely(ret == -1 && rte_errno == ENOSPC)) {
            /**
                * Early pkts just outside of window should be dropped
                */
            printf("%s():No space in reorder buffer, seq=%d\n", __func__,*rte_reorder_seqn(mbuf[i]));

        }
        // debug 
        // else if(*rte_reorder_seqn(mbuf[i]) < 512){
        //     //printf("%s():Succesfully insert mbuf into reorder buf, seq=%d\n", __func__,*rte_reorder_seqn(mbuf[i]));
        //     ;
        // }
    }
        
        // debug 
        // printf("tot_enq %d\n",tot_enq);
        // printf("nb_mbuf %d\n",nb_mbuf);
        //printf("dret %d\n",dret);
    
    //mystate->nb_staged_buf += nb_mbuf;
    
    /*
    * drain batch_size of reordered mbufs to leave pipeline processing
    */
    
    dret = rte_reorder_drain(mystate->reorder_buf, mbuf_drain, batch_size);
    *nb_deq = dret;

    // debug
    // if(nb_mbuf>0 && *rte_reorder_seqn(mbuf[0]) < 1024){
    // //if(nb_mbuf>0){
    //     printf("dret = %d\n",dret);
    // }
    //printf("dret = %d\n",dret);

    #ifdef REORDER_VERIFY_ON
    reorder_verify(self, mbuf_drain, dret);
    #endif

    return 0;
}


int reorder_verify(struct pipeline_stage *self, struct rte_mbuf **mbuf, int nb_mbuf){
    
    struct reorder_state *mystate = (struct reorder_state *)self->state;


    if(nb_mbuf == 0){
        return 0;
    }

    if((*rte_reorder_seqn(mbuf[0])) != mystate->last_seq_nb+1){
        MEILI_LOG_ERR("Error seq order, seq_num=%d, %d", mystate->last_seq_nb,*rte_reorder_seqn(mbuf[0]));
    }

    for(int i=1; i<nb_mbuf; i++){
        if((*rte_reorder_seqn(mbuf[i-1])+1) != *rte_reorder_seqn(mbuf[i])){
            MEILI_LOG_ERR("Error seq order, seq_num=%d, %d", *rte_reorder_seqn(mbuf[i-1]),*rte_reorder_seqn(mbuf[i]));
        }  
    }		
    
    mystate->last_seq_nb = *rte_reorder_seqn(mbuf[nb_mbuf-1]);

    return 0;
}


