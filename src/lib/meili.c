/* Copyright (c) 2024, Meili Authors */

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/socket.h>
#include <resolv.h>
#include <sys/epoll.h>
#include <arpa/inet.h>

#include "meili.h"
#include "../runtime/pipeline.h"

#include "./net/meili_pkt.h"
#include "./regex/meili_regex.h"
#include "./log/meili_log.h"

/* pkt_trans
*   - Run a packet transformation operation specified by UCO.  
*/
void pkt_trans(struct pipeline_stage *self, int (*trans)(struct pipeline_stage *self, meili_pkt *pkt), meili_pkt *pkt){
    if(!trans){
        return;   
    }
    trans(self, pkt);
};

/* pkt_flt
*   - Filter packets with the operation specified by UCO.
*/
void pkt_flt(struct pipeline_stage *self, int (*check)(struct pipeline_stage *self, meili_pkt *pkt), meili_pkt *pkt){
    // printf("Meili api pkt_lt called\n");
    if(!check){
        return; 
    }
    int flag = check(self, pkt);

    if(flag == 1){
        /* filter the packet */
        ;
    }
}

/* flow_ext
*   - Construct flows from a stream based on UCO.
*/
void flow_ext(){};

/* flow_trans
*   - Run a flow transformation operation specified by UCO.
*/
void flow_trans(){};

/* reg_sock
*   - Register an established socket to Meili.
*/
int reg_sock(struct pipeline_stage *self){
    int sockfd;
    int epfd;
    struct epoll_event event;

    if ( (sockfd = socket(AF_INET, SOCK_STREAM|SOCK_NONBLOCK, 0)) < 0 ) {
        MEILI_LOG_ERR("Socket registration failed");
        return -EINVAL;
    }

    self->sockfd = sockfd;
    epfd = epoll_create(1);
    event.events = EPOLLIN; 
    event.data.fd = sockfd;

    epoll_ctl(epfd, EPOLL_CTL_ADD, sockfd, &event);

    return 0;
};

/* epoll
*   - Process an event on the socket with the operation specified by UCO.
*/
void epoll(struct pipeline_stage *self, void (*epoll_process)(char *buffer, int buf_len), int event){
    
    int num_events;   
    struct epoll_event events[MEILI_MAX_EPOLL_EVENTS];
    char buffer[MEILI_EPOLL_BUF_SIZE];
    int buf_len;
     
    num_events = epoll_wait(self->epfd, events, MEILI_MAX_EPOLL_EVENTS, MEILI_EPOLL_TIMEOUT);

    for(int i = 0; i < num_events; i++) {
        if(events[i].events & event) {
            buf_len = read(self->sockfd, buffer, sizeof(buffer));
            epoll_process(buffer, buf_len);
        }
    }

};

/* regex
*   - The built-in Regular Expression API.   
*/
void regex(struct pipeline_stage *self, meili_pkt *pkt){
    
    int qid = self->worker_qid;
    struct pipeline *pl = (struct pipeline *)(self->pl);
    regex_stats_t temp_stats;
    pl_conf *run_conf = &(pl->conf);

	int to_send = 0;
	int ret;
    int nb_dequeued_op = 0;


    /* Prepare ops in regex_dev_search_live */
    to_send = regex_dev_search_live(run_conf, qid, pkt, &temp_stats);
    // if (ret)
    //     return ret;

    /* If to_send signal is set, push the batch( and pull at the same time to avoid full queue) */
    if (to_send) {
        regex_dev_force_batch_push(run_conf, qid, &temp_stats, &nb_dequeued_op, NULL);
    }	
	else{
		/* If batch is not full, pull finished ops */
		regex_dev_force_batch_pull(run_conf, qid, &temp_stats, &nb_dequeued_op, NULL);	
	}
	return;        
};

/* AES
*   - The built-in AES Encryption API.
*/
void AES(){};

/* compression
*   - The built-in Compression API.
*/
void compression(){};

int register_meili_apis(){
    printf("register meili apis\n");
    Meili.pkt_trans     = pkt_trans;
    Meili.pkt_flt       = pkt_flt;
    Meili.flow_ext      = flow_ext;
    Meili.flow_trans    = flow_trans; 
    Meili.reg_sock      = reg_sock;
    Meili.epoll         = epoll;
    Meili.regex         = regex;
    Meili.AES           = AES;
    Meili.compression   = compression;
    return 0;
}