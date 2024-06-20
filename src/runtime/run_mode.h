/*
 * Copyright (c) 2021-2022 NVIDIA CORPORATION & AFFILIATES, ALL RIGHTS RESERVED.
 *
 * This software product is a proprietary product of NVIDIA CORPORATION &
 * AFFILIATES (the "Company") and all right, title, and interest in and to the
 * software product, including all associated intellectual property rights, are
 * and shall remain exclusively with the Company.
 *
 * This software product is governed by the End User License Agreement
 * provided with the software product.
 *
 */
/* Modified by Meili Authors */ 
/* Copyright (c) 2024, Meili Authors */

#ifndef _INCLUDE_RUN_H_
#define _INCLUDE_RUN_H_

#include <stdio.h>

#include <rte_malloc.h>

#include "pipeline.h"

#include "../utils/utils.h"

// #include "../utils/conf/conf.h"
// #include "../lib/log/meili_log.h"

#define DEFAULT_ETH_BATCH_SIZE 64

/* rte ring working mode */
//#define SHARED_BUFFER

/* defines in set 0 is compatible with defines in set 1 */
/* exclusvie runing modes set 0 */
//#define RATE_LIMIT_BPS_ON 	/* throughput mode with bps rate limit on */ 
//#define RATE_LIMIT_PPS_ON		/* throughput mode with pps rate limit on */  
//#define LATENCY_MODE_ON 		/* per pkt latency measurement mode */  
//#define ONLY_MAIN_MODE_ON		/* only running main loop without enqueue/dequeue */ 

/* exclusvie runing modes set 1 */
// #define ALL_REMOTE_ON_ARRIVAL 			/* direct all traffic to remote pipelines right after receiving them */
//#define ALL_REMOTE_AFTER_PROCESSING		/* direct all traffic to remote pipelines after processing them locally */
#define MEILI_MODE
// #define BASELINE_MODE

#ifdef RATE_LIMIT_BPS_ON   
#define RATE_Gbps 10
#endif

#ifdef RATE_LIMIT_PPS_ON  
#define RATE_Mpps 1
#endif

#ifdef LATENCY_MODE_ON
	/* which latency to record */
	#define LATENCY_END2END
	//#define LATENCY_PARTITION
	//#define LATENCY_AGGREGATION 

	/* latency sampling mode to view tail latency */
	#define PKT_LATENCY_SAMPLE_ON
	//#define PKT_LATENCY_BREAKDOWN_ON

	/* batch size - per-pkt processing */
	#define DEFAULT_BATCH_SIZE 1
#else
	//test
	//#define LATENCY_END2END
	//#define LATENCY_PARTITION
	//#define LATENCY_AGGREGATION
	//#define PKT_LATENCY_SAMPLE_ON
	#define DEFAULT_BATCH_SIZE 64
#endif



extern volatile bool force_quit;

typedef struct run_func {
	int (*run)(struct pipeline *pl);
} run_func_t;

void run_local_reg(run_func_t *funcs);

void run_dpdk_reg(run_func_t *funcs);

/* Register run mode functions as dicatated by input mode selected. */
static inline int
run_mode_register(struct pipeline *pl)
{
	run_func_t *funcs;
	pl_conf *run_conf = &pl->conf;

	funcs = rte_zmalloc(NULL, sizeof(run_func_t), 0);
	if (!funcs) {
		MEILI_LOG_ERR("Memory failure in run mode register.");
		return -ENOMEM;
	}

	switch (run_conf->input_mode) {
	case INPUT_TEXT_FILE:
	case INPUT_PCAP_FILE:
	case INPUT_JOB_FORMAT:
	case INPUT_REMOTE_MMAP:
		run_local_reg(funcs);
		break;

	case INPUT_LIVE:
		run_dpdk_reg(funcs);
		break;

	default:
		rte_free(funcs);
		return -ENOTSUP;
	}

	run_conf->run_funcs = funcs;

	return 0;
}

static inline int
run_mode_launch(struct pipeline *pl)
{
	run_func_t *funcs = pl->conf.run_funcs;

	if (funcs->run){
		return funcs->run(pl);
	}

	return -EINVAL;
}

#endif /* _INCLUDE_RUN_H_ */
