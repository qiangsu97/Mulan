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

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <rte_common.h>
#include <rte_cycles.h>
#include <rte_eal.h>
#include <rte_errno.h>
#include <rte_timer.h>

#include "run_mode.h"
#include "meili_runtime.h"

#include "../utils/utils.h"
#include "../utils/input_mode/input.h"

volatile bool force_quit;

// struct lcore_worker_args {
// 	pl_conf *run_conf;
// 	uint32_t qid;
// };


static void
signal_handler(int signum)
{
	if (signum == SIGINT || signum == SIGTERM) {
		MEILI_LOG_INFO("Signal %d received, preparing to exit...", signum);
		force_quit = true;
	}
}


int
main(int argc, char **argv)
{
	uint64_t start_cycles, end_cycles;
	char err[ERR_STR_SIZE] = {0};
	pl_conf *run_conf;


	unsigned int lcore_id;
	uint32_t worker_qid;
	rb_stats_t *stats;
	
	double run_time;
	int ret;

	struct pipeline pl;

	
	run_conf = &(pl.conf);
	memset(run_conf, 0, sizeof(pl_conf)); 

	
	force_quit = false;

	/* register handlers */
	signal(SIGINT, signal_handler);
	signal(SIGTERM, signal_handler);


	/* initialize config indicated by input command */
	ret = conf_setup(run_conf, argc, argv);
	if (ret){
		rte_exit(EXIT_FAILURE, "Configuration error\n");
	}

	ret = register_meili_apis();
	if (ret){
		rte_exit(EXIT_FAILURE, "Meili API registration failed\n");
	}

	ret = meili_runtime_init(&pl, run_conf, err);
	if(ret){
		goto end;
	}

	run_conf->shinfo.free_cb = extbuf_free_cb;


	/* Main core gets regex queue 0 and stats position 0. */
	stats = run_conf->stats;
	stats->rm_stats[0].lcore_id = rte_get_main_lcore();


	MEILI_LOG_INFO("Beginning Processing...");
	start_cycles = rte_get_timer_cycles();

	/* Start each worker lcore and then main core. */
	/* Each pipeline stage is assigned to a worker */
	ret = pipeline_run(&pl);

	end_cycles = rte_get_timer_cycles();
	run_time = ((double)end_cycles - start_cycles) / rte_get_timer_hz();

	stats_print_end_of_run(run_conf, run_time);


// clean_regex:
// 	regex_dev_clean_regex(run_conf);
clean_pipeline:
	pipeline_free(&pl);
clean_input:
	input_clean(run_conf);
clean_stats:
	stats_clean(run_conf);
clean_conf:
	conf_clean(run_conf);

end:
	if (ret){
		rte_exit(EXIT_FAILURE, "%s\n", err);
	}
		
	// else{
	// 	rte_eal_cleanup();
	// }
	
	return ret;
}
