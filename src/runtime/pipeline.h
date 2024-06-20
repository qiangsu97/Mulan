/* Copyright (c) 2024, Meili Authors */

#ifndef _INCLUDE_PIPELINE_H
#define _INCLUDE_PIPELINE_H

#include <rte_mbuf.h>
#include <sys/socket.h>
#include <resolv.h>
#include <sys/epoll.h>
#include <arpa/inet.h>

#include "../lib/conf/meili_conf.h"

#include "../lib/net/meili_pkt.h"


#define MEILI_MAX_EPOLL_EVENTS 1024
#define MEILI_EPOLL_TIMEOUT 1024
#define MEILI_EPOLL_BUF_SIZE 1024
/* memory pool macros for local mode */
#define MBUF_POOL_SIZE		     1048576-1 /* Should be n = (2^q - 1)*/
//#define MBUF_POOL_SIZE		     131072-1 
//#define MBUF_POOL_SIZE		     65536-1 
#define MBUF_CACHE_SIZE		     0
#define MBUF_SIZE		         2048


/* ring and batch macros */
#define NB_MAX_RING 16
#define MAX_PKTS_BURST 8192
#define RING_SIZE 8192

// /* Note that when batch size of pipeline processing is too small, sending rate can not be high or mbuf pool of rx/tx queue is not big enough */
// /* for example, 1500 byte pkt will trigger this issue with 1Kpps */
// /* Advice: only use small batch size for measuring per-pkt latency, never use it for normal pipeline processing */

/* pl stage macros */
#define NB_PIPELINE_STAGE_MAX 8
#define NB_INSTANCE_PER_PIPELINE_STAGE_MAX 8

/* error message macros */
#define ERR_STR_SIZE 50

/* pl topo */
#define CONFIG_BUF_LEN 512
#define PL_CONFIG_PATH "./src/pl.conf"


enum pipeline_type {
    PL_ECHO,
	PL_DDOS,
    PL_REGEX_BF,
    PL_COMPRESS_BF,
    PL_AES,
    PL_SHA,
    PL_FIREWALL_ACL,
    PL_MONITOR_CMS,
    PL_MONITOR_HLL,
    PL_L3_LOAD_BALANCER,
    PL_API_GATEWAY,
    PL_HTTP_PARSER,
    /* test (composed) applications */
    /* packet processing(L3) */
    PL_APP_IDS,
    PL_APP_IPCOMP_GATEWAY,
    PL_APP_IPSEC_GATEWAY,
    PL_APP_FIREWALL,
    PL_APP_FLOW_MONITOR,
    /* socket processing(L7) */
    PL_APP_API_GATEWAY,
    PL_APP_L7_LOAD_BALANCER,
    /* end of test (composed) applications */
	PL_MAIN,
    PL_NB_OF_STAGE_TYPES
};

#define PRINT_STAGE_TYPE(x) switch(x){\
                        case PL_ECHO:                   printf("%16s","PL_ECHO");break;\
                        case PL_DDOS:                   printf("%16s","PL_DDOS");break;\
                        case PL_REGEX_BF:               printf("%16s","PL_REGEX_BF");break;\
                        case PL_COMPRESS_BF:            printf("%16s","PL_COMPRESS_BF");break;\
                        case PL_AES:                    printf("%16s","PL_AES");break;\
                        case PL_SHA:                    printf("%16s","PL_SHA");break;\
                        case PL_FIREWALL_ACL:           printf("%16s","PL_FIREWALL_ACL");break;\
                        case PL_MONITOR_CMS:            printf("%16s","PL_MONITOR_CMS");break;\
                        case PL_MONITOR_HLL:            printf("%16s","PL_MONITOR_HLL");break;\
                        case PL_L3_LOAD_BALANCER:       printf("%16s","PL_L3_LOAD_BALANCER");break;\
                        case PL_API_GATEWAY:            printf("%16s","PL_API_GATEWAY");break;\
                        case PL_HTTP_PARSER:            printf("%16s","PL_HTTP_PARSER");break;\
                        case PL_APP_IDS:                printf("%16s","PL_APP_IDS");break;\
                        case PL_APP_IPCOMP_GATEWAY:     printf("%16s","PL_APP_IPCOMP_GATEWAY");break;\
                        case PL_APP_IPSEC_GATEWAY:      printf("%16s","PL_APP_IPSEC_GATEWAY");break;\
                        case PL_APP_FIREWALL:           printf("%16s","PL_APP_FIREWALL");break;\
                        case PL_APP_FLOW_MONITOR:       printf("%16s","PL_APP_FLOW_MONITOR");break;\
                        case PL_APP_API_GATEWAY:        printf("%16s","PL_APP_API_GATEWAY");break;\
                        case PL_APP_L7_LOAD_BALANCER:   printf("%16s","PL_APP_L7_LOAD_BALANCER");break;\
                        case PL_MAIN:                   printf("%16s","PL_MAIN");break;\
                        default:                        printf("%16s","Unknown");break;\
                        }

#define GET_STAGE_TYPE_STRING(x,y) switch(x){\
                        case PL_ECHO:                   sprintf(y,"PL_ECHO");break;\
                        case PL_DDOS:                   sprintf(y,"PL_DDOS");break;\
                        case PL_REGEX_BF:               sprintf(y,"PL_REGEX_BF");break;\
                        case PL_COMPRESS_BF:            sprintf(y,"PL_COMPRESS_BF");break;\
                        case PL_AES:                    sprintf(y,"PL_AES");break;\
                        case PL_SHA:                    sprintf(y,"PL_SHA");break;\
                        case PL_FIREWALL_ACL:           sprintf(y,"PL_FIREWALL_ACL");break;\
                        case PL_MONITOR_CMS:            sprintf(y,"PL_MONITOR_CMS");break;\
                        case PL_MONITOR_HLL:            sprintf(y,"PL_MONITOR_HLL");break;\
                        case PL_L3_LOAD_BALANCER:       sprintf(y,"PL_L3_LOAD_BALANCER");break;\
                        case PL_API_GATEWAY:            sprintf(y,"PL_API_GATEWAY");break;\
                        case PL_HTTP_PARSER:            sprintf(y,"PL_HTTP_PARSER");break;\
                        case PL_APP_IDS:                sprintf(y,"PL_APP_IDS");break;\
                        case PL_APP_IPCOMP_GATEWAY:     sprintf(y,"PL_APP_IPCOMP_GATEWAY");break;\
                        case PL_APP_IPSEC_GATEWAY:      sprintf(y,"PL_APP_IPSEC_GATEWAY");break;\
                        case PL_APP_FIREWALL:                 sprintf(y,"PL_APP_FIREWALL");break;\
                        case PL_APP_FLOW_MONITOR:       sprintf(y,"PL_APP_FLOW_MONITOR");break;\
                        case PL_APP_API_GATEWAY:        sprintf(y,"PL_APP_API_GATEWAY");break;\
                        case PL_APP_L7_LOAD_BALANCER:   sprintf(y,"PL_APP_L7_LOAD_BALANCER");break;\
                        case PL_MAIN:                   sprintf(y,"PL_MAIN");break;\
                        default:                        sprintf(y,"Unknown");break;\
                        }


#define GET_STAGE_TYPE_NUMBER(x,y)  if(strcmp(x,"PL_ECHO")==0)                      {*y = PL_ECHO;}\
                                    else if(strcmp(x,"PL_DDOS")==0)                 {*y = PL_DDOS;}\
                                    else if(strcmp(x,"PL_REGEX_BF")==0)             {*y = PL_REGEX_BF;}\
                                    else if(strcmp(x,"PL_COMPRESS_BF")==0)          {*y = PL_COMPRESS_BF;}\
                                    else if(strcmp(x,"PL_AES")==0)                  {*y = PL_AES;}\
                                    else if(strcmp(x,"PL_SHA")==0)                  {*y = PL_SHA;}\
                                    else if(strcmp(x,"PL_FIREWALL_ACL")==0)         {*y = PL_FIREWALL_ACL;}\
                                    else if(strcmp(x,"PL_MONITOR_CMS")==0)          {*y = PL_MONITOR_CMS;}\
                                    else if(strcmp(x,"PL_MONITOR_HLL")==0)          {*y = PL_MONITOR_HLL;}\
                                    else if(strcmp(x,"PL_L3_LOAD_BALANCER")==0)     {*y = PL_L3_LOAD_BALANCER;}\
                                    else if(strcmp(x,"PL_API_GATEWAY")==0)          {*y = PL_API_GATEWAY;}\
                                    else if(strcmp(x,"PL_HTTP_PARSER")==0)          {*y = PL_HTTP_PARSER;}\
                                    else if(strcmp(x,"PL_APP_IDS")==0)              {*y = PL_APP_IDS;}\
                                    else if(strcmp(x,"PL_APP_IPCOMP_GATEWAY")==0)   {*y = PL_APP_IPCOMP_GATEWAY;}\
                                    else if(strcmp(x,"PL_APP_IPSEC_GATEWAY")==0)    {*y = PL_APP_IPSEC_GATEWAY;}\
                                    else if(strcmp(x,"PL_APP_FIREWALL")==0)         {*y = PL_APP_FIREWALL;}\
                                    else if(strcmp(x,"PL_APP_FLOW_MONITOR")==0)     {*y = PL_APP_FLOW_MONITOR;}\
                                    else if(strcmp(x,"PL_APP_API_GATEWAY")==0)      {*y = PL_APP_API_GATEWAY;}\
                                    else if(strcmp(x,"PL_APP_L7_LOAD_BALANCER")==0) {*y = PL_APP_L7_LOAD_BALANCER;}\
                                    else if(strcmp(x,"PL_MAIN")==0)                 {*y = PL_MAIN;}\
                                    else{*y = -1;}

struct pipeline_stage{
    void *apis;

    enum pipeline_type type;    /* stage workload type */
    //bool push_batch;
    void *state;                /* stage private state */
    int core_id;                /* stage core id */
    int worker_qid;             /* stage qid */
    int batch_size;             /* stage batch size */

    #ifdef SHARED_BUFFER 
    /* i/o buffer */
    struct rte_ring *ring_in;   /* stage input ring */
	struct rte_ring *ring_out;  /* stage output ring */
    #else
    /* i/o buffer for first input and final output */
    struct rte_ring *ring_in[NB_MAX_RING];
	struct rte_ring *ring_out[NB_MAX_RING];
    int nb_ring_in;
    int nb_ring_out;
    #endif

    /* socket processing */
    int sockfd;
    int epfd;
    struct epoll_event events[MEILI_MAX_EPOLL_EVENTS];

    /* timestamping for this stage */
    int ts_start_offset;
    int ts_end_offset;

    /* functions for this pipeline stage */
    struct pipeline_func *funcs;/* stage operator functions */

    /* regex related confs */
    void *regex_conf;

    /* parent */
    void *pl;                   /* parent pipeline structure */

};


/* pipeline */
struct pipeline{
    /* fields for pipeline information */
    struct pipeline_stage *stages[NB_PIPELINE_STAGE_MAX][NB_INSTANCE_PER_PIPELINE_STAGE_MAX]; 
    enum pipeline_type stage_types[NB_PIPELINE_STAGE_MAX];
    int nb_inst_per_pl_stage[NB_PIPELINE_STAGE_MAX];
    int nb_pl_stages;
    int nb_pl_stage_inst;

    /* mempool for storing preloaded mbufs in preloaded mode */
    struct rte_mempool *mbuf_pool;

    #ifdef SHARED_BUFFER 
    // /* i/o buffer for first input and final output */
    struct rte_ring *ring_in;
	struct rte_ring *ring_out;
    #else
    /* i/o buffer for first input and final output */
    struct rte_ring *ring_in[NB_MAX_RING];
	struct rte_ring *ring_out[NB_MAX_RING];
    #endif

    /* run config read from command line options */
    pl_conf conf;

    /* timestamping for this stage */
    int ts_start_offset;
    int ts_end_offset;

    /* sepcial stages: sequencing and reordering */
    struct pipeline_stage seq_stage;
    struct pipeline_stage reorder_stage;

};


/* Function pointers each pipeline stage should implement. 
   1. pipeline_stage_exec: process total number of nb_enq mbufs in mbuf, and store the number of mbufs in *nb_deq, and corresponding mbufs in *mbuf_out.
      For sequential processing pl stages(i.e. ddos), to avoid copying mbuf pointers from mbuf to *mbuf_out, simple change the value of *mbuf_out to mbuf 


*/

typedef struct pipeline_func {
    int (*pipeline_stage_init)(struct pipeline_stage *self);
    int (*pipeline_stage_free)(struct pipeline_stage *self);
    // int (*pipeline_stage_exec)(struct pipeline_stage *self, 
    //                         struct rte_mbuf **mbuf,
    //                         int nb_enq,
    //                         struct rte_mbuf ***mbuf_out,
    //                         int *nb_deq);
    int (*pipeline_stage_exec)(struct pipeline_stage *self, meili_pkt *pkt);
} pipeline_func_t;



/* register functions */
int meili_pipeline_stage_func_reg(struct pipeline_stage *stage);
//int seq_pipeline_stage_func_reg(struct pipeline_stage *stage);
//int reorder_pipeline_stage_func_reg(struct pipeline_stage *stage);

// int echo_pipeline_stage_func_reg(struct pipeline_stage *self);
// int ddos_pipeline_stage_func_reg(struct pipeline_stage *self);
// int regex_bf_pipeline_stage_func_reg(struct pipeline_stage *stage);
// int compress_bf_pipeline_stage_func_reg(struct pipeline_stage *stage);
// int aes_pipeline_stage_func_reg(struct pipeline_stage *stage);
// int sha_pipeline_stage_func_reg(struct pipeline_stage *stage);
// int firewall_acl_pipeline_stage_func_reg(struct pipeline_stage *stage);
// int monitor_cms_pipeline_stage_func_reg(struct pipeline_stage *stage);
// int monitor_hll_pipeline_stage_func_reg(struct pipeline_stage *stage);
// int l3_lb_pipeline_stage_func_reg(struct pipeline_stage *stage);
// int api_gw_pipeline_stage_func_reg(struct pipeline_stage *stage);
// int http_parser_pipeline_stage_func_reg(struct pipeline_stage *stage);
// /* applications */
// int app_ids_pipeline_stage_func_reg(struct pipeline_stage *stage);
// int app_ipcomp_gw_pipeline_stage_func_reg(struct pipeline_stage *stage);
// int app_ipsec_gw_pipeline_stage_func_reg(struct pipeline_stage *stage);
// int app_fw_pipeline_stage_func_reg(struct pipeline_stage *stage);
// int app_flow_mon_pipeline_stage_func_reg(struct pipeline_stage *stage);
// int app_api_gw_pipeline_stage_func_reg(struct pipeline_stage *stage);
// int app_l7_lb_pipeline_stage_func_reg(struct pipeline_stage *stage);


/* general functions for pipeline stages */
int pipeline_stage_init_safe(struct pipeline_stage *self, enum pipeline_type pp_type);
int pipeline_stage_free_safe(struct pipeline_stage *self);
// int pipeline_stage_exec_safe(struct pipeline_stage *self, 
//                             struct rte_mbuf **mbuf,
//                             int nb_enq,
//                             struct rte_mbuf ***mbuf_out,
//                             int *nb_deq);
int pipeline_stage_run_safe(struct pipeline_stage *self);

/* functions for pipelines */
int pipeline_init_safe(struct pipeline *pl);
// int pipeline_init_safe(struct pipeline *pl, char *config_path);
int pipeline_free(struct pipeline *pl);
int pipeline_run(struct pipeline *pl);



void extbuf_free_cb(void *addr __rte_unused, void *fcb_opaque __rte_unused);

#endif /* _INCLUDE_PIPELINE_H */
