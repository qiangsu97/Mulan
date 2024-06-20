/* Copyright (c) 2024, Meili Authors */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <rte_errno.h>

#include "pipeline.h"
#include "run_mode.h"
#include "../utils/utils.h"

#include "../packet_ordering/packet_ordering.h"
#include "../packet_timestamping/packet_timestamping.h"
#include "../utils/input_mode/input.h"

static int
init_dpdk(pl_conf *run_conf)
{
	int ret;

	if (run_conf->dpdk_argc <= 1) {
		MEILI_LOG_ERR("Too few DPDK parameters.");
		return -EINVAL;
	}

	ret = rte_eal_init(run_conf->dpdk_argc, run_conf->dpdk_argv);

	/* Return num of params on success. */
	return ret < 0 ? -rte_errno : 0;
}

/* Initialization function of Meili runtime. Initializations include:
    1. DPDK EAL
    2. Status structure allocation
    3. TO initialization
    4. Accelerator initialization, i.e. regex, compression
    5. Pipeline stage allocation and topology construction
*/
int meili_runtime_init(struct pipeline *pl, pl_conf *run_conf, char *err){
    int ret = 0;

    /* initalize dpdk related environment */
    ret = init_dpdk(run_conf);
	if (ret) {
		snprintf(err, ERR_STR_SIZE, "Failed to init DPDK");
		goto clean_conf;
	}

	/* Confirm there are enough DPDK lcores for user core request. */
	if (run_conf->cores > rte_lcore_count()) {
		MEILI_LOG_WARN_REC(run_conf, "requested cores (%d) > dpdk lcores (%d) - using %d.", run_conf->cores,
				  rte_lcore_count(), rte_lcore_count());
		run_conf->cores = rte_lcore_count();
	}


    /* init global stats recording structures */
    /* TODO: add regex related structures */
	ret = stats_init(run_conf);
	if (ret) {
		snprintf(err, ERR_STR_SIZE, "Failed initialising stats");
		goto clean_conf;
	}

    /* Register input init function according to command line arguments. Input methods can be 
     * 1) INPUT_TEXT_FILE       Load txt file into memory. Note that for txt files, it may take up large space.
     * 2) INPUT_PCAP_FILE       Load pcap file into memory. Note that for pcap files, it may take up large space.
     * 3) INPUT_LIVE            Use dpdk port to receive pkts. 
     * 4) INPUT_JOB_FORMAT      N/A
     * 5) INPUT_REMOTE_MMAP     N/A
    */
	ret = input_register(run_conf);
	if (ret) {
		snprintf(err, ERR_STR_SIZE, "Input registration error");
		goto clean_stats;
	}
    /* set up input configurations */
	ret = input_init(run_conf);
	if (ret) {
		snprintf(err, ERR_STR_SIZE, "Input method initialization failed");
		goto clean_stats;
	}

    /* Init regex device */
    ret = meili_regex_init(run_conf);
    if (ret) {
        goto clean_input;    
    }

    /* construct pipeline topo */
	/* populate pipeline fields first */
	// pl.nb_pl_stages = 2;
	// pl.stage_types[0] = PL_REGEX_BF;
	// pl.stage_types[1] = PL_DDOS;
    // pl.nb_inst_per_pl_stage[0] = 1;
	// pl.nb_inst_per_pl_stage[1] = 4;

    /* construct pipeline topo based on pl.conf file */
    /* TODO: add regex related structures */
	// ret = pipeline_init_safe(pl, PL_CONFIG_PATH);
    ret = pipeline_init_safe(pl);
	if (ret) {
		snprintf(err, ERR_STR_SIZE, "Pipeline initialising failed");
		
		goto clean_pipeline;
	}

    /* register main thread run function based on input mode (local txt/pcap, dpdk port, ...) */
    ret = run_mode_register(pl);
	if (ret) {
		snprintf(err, ERR_STR_SIZE, "Run mode registration error");
		goto clean_pipeline;
	}

    MEILI_LOG_INFO("Pipeline runtime initalization finished");
    goto end;

clean_pipeline:
	pipeline_free(pl);
clean_regex:
clean_input:
	input_clean(run_conf);
clean_stats:
	stats_clean(run_conf);
clean_conf:
	conf_clean(run_conf);
end:
    return ret;
}