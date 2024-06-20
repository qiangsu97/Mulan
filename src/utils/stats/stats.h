/* Copyright (c) 2024, Meili Authors */

#ifndef _INCLUDE_STATS_H_
#define _INCLUDE_STATS_H_

#include <stdint.h>

#include <rte_cycles.h>
#include <rte_ether.h>
#include <rte_ip.h>
#include <rte_tcp.h>
#include <rte_udp.h>

#include "../../lib/conf/meili_conf.h"
#include "../../runtime/meili_runtime.h"

// #define ONLY_SPLIT_THROUGHPUT

#define STATS_INTERVAL_SEC	1
#define STATS_INTERVAL_CYCLES	STATS_INTERVAL_SEC * rte_get_timer_hz()

#define NUMBER_OF_SAMPLE ((1<<14) - 1) /* should be 2^n-1 for speed */ 

typedef struct pkt_stats {
	uint64_t valid_pkts;	   /* Successfully parsed. */
	uint64_t unsupported_pkts; /* Unrecognised protocols. */
	uint64_t no_payload;	   /* TCP ack or similar. */
	uint64_t invalid_pkt;	   /* Bad cap length. */
	uint64_t thres_drop;	   /* Payload < input threshold. */

	/* Protocol counters. */
	uint64_t vlan;
	uint64_t qnq;
	uint64_t ipv4;
	uint64_t ipv6;
	uint64_t tcp;
	uint64_t udp;
} pkt_stats_t;



/* Stats per core are stored as an array. */
typedef struct run_mode_stats {
	union {
		struct {
			int lcore_id;
			uint64_t rx_buf_cnt;   /* Data received. */
			uint64_t rx_buf_bytes; /* Bytes received. */
			uint64_t rx_batch_cnt; /* Batches sent. */
			uint64_t tx_buf_cnt;   /* Data sent. */
			uint64_t tx_buf_bytes; /* Bytes sent. */
			uint64_t tx_batch_cnt; /* Batches sent. */
			uint64_t split_tx_buf_bytes;  /* Bytes last recorded. */
			uint64_t split_tx_buf_cnt;  /* Buf last recorded. */
			double split_duration;  /* per core duration recording. */

			pkt_stats_t pkt_stats; /* Packet stats. */

			struct pipeline_stage *self;/* corresponding pipeline stage */
		};
		/* Ensure multiple cores don't access the same cache line. */
		unsigned char cache_align[CACHE_LINE_SIZE * 2];
	};
} run_mode_stats_t;


typedef struct lat_stats {
	uint64_t tot_lat;
	uint64_t max_lat;
	uint64_t min_lat;
	/* queuing time before get processed by this stage */
	uint64_t tot_in_lat;
	// sample 
	uint64_t time_diff_sample[NUMBER_OF_SAMPLE];
	int nb_sampled;
} lat_stats_t;

typedef struct rxpbench_stats {
	run_mode_stats_t *rm_stats;
	lat_stats_t *lat_stats;
} rb_stats_t;

/* Modify packet stats (common to live and pcap modes). */
static inline void
stats_update_pkt_stats(pkt_stats_t *pkt_stats, int rte_ptype)
{
	pkt_stats->valid_pkts++;
	if ((rte_ptype & RTE_PTYPE_L2_MASK) == RTE_PTYPE_L2_ETHER_VLAN)
		pkt_stats->vlan++;
	else if ((rte_ptype & RTE_PTYPE_L2_MASK) == RTE_PTYPE_L2_ETHER_QINQ)
		pkt_stats->qnq++;

	if (RTE_ETH_IS_IPV4_HDR(rte_ptype))
		pkt_stats->ipv4++;
	else if (RTE_ETH_IS_IPV6_HDR(rte_ptype))
		pkt_stats->ipv6++;

	if ((rte_ptype & RTE_PTYPE_L4_MASK) == RTE_PTYPE_L4_TCP)
		pkt_stats->tcp++;
	else if ((rte_ptype & RTE_PTYPE_L4_MASK) == RTE_PTYPE_L4_UDP)
		pkt_stats->udp++;
}

int stats_init(pl_conf *run_conf);
void stats_clean(pl_conf *run_conf);
void stats_print_update(rb_stats_t *stats, int num_queues, double time, bool end);
void stats_print_end_of_run(pl_conf *run_conf, double run_time);
void stats_update_time_main(struct rte_mbuf **mbuf, int nb_mbuf, struct pipeline *pl);


#endif /* _INCLUDE_STATS_H_ */
